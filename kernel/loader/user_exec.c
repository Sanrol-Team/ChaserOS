#include "loader/user_exec.h"

#include "errno.h"
#include "fs/vfs.h"
#include "loader/cndyn_loader.h"
#include "process.h"
#include "sched.h"
#include "user.h"
#include "vmm.h"

#include <stddef.h>
#include <stdint.h>

#define USER_EXEC_STACK_TOP 0x801008ULL

static long copy_user_path(char *dst, size_t cap, uint64_t ua) {
    if (!dst || cap == 0) {
        return -EINVAL;
    }
    for (size_t i = 0; i + 1 < cap; i++) {
        if (!vmm_user_range_readable(ua + i, 1)) {
            return -EFAULT;
        }
        char c = *(const volatile char *)(uintptr_t)(ua + i);
        dst[i] = c;
        if (c == '\0') {
            return 0;
        }
    }
    dst[cap - 1] = '\0';
    return -EINVAL;
}

static proc_t *process_current_user_or_kernel(void) {
    proc_t *u = process_find_by_pid(chaseros_active_user_pid);
    if (u) {
        return u;
    }
    return get_current_process();
}

static long resolve_user_path(uint64_t user_path_ptr, char *out, size_t out_cap) {
    char rel[160];
    long r = copy_user_path(rel, sizeof(rel), user_path_ptr);
    if (r != 0) {
        return r;
    }
    if (!vfs_is_mounted()) {
        return -EINVAL;
    }
    if (vfs_resolve_path(chaseros_user_get_cwd(), rel, out, out_cap) != 0) {
        return -EINVAL;
    }
    return 0;
}

long user_sys_spawn(uint64_t user_path_ptr) {
    char path[256];
    uint64_t entry = 0;
    proc_t *parent = process_current_user_or_kernel();
    long rr = resolve_user_path(user_path_ptr, path, sizeof(path));
    if (rr != 0) {
        return rr;
    }
    proc_t *child = process_alloc_user(parent ? parent->pid : 0);
    if (!child) {
        return -EAGAIN;
    }
    if (cndyn_load_package_for_exec(path, &entry) != 0) {
        child->state = PROC_STATE_FREE;
        return -ENOENT;
    }
    child->user_entry = entry;
    child->user_stack_top = USER_EXEC_STACK_TOP;
    /*
     * 当前内核仍是单用户槽位执行模型：先登记子进程元数据并标记可回收，
     * waitpid 可以完成父子状态闭环；后续 fork/exec 完整调度再改为真正并发执行。
     */
    process_mark_exit(child, 0);
    return (long)child->pid;
}

long user_sys_exec(uint64_t user_path_ptr, struct registers *regs) {
    char path[256];
    uint64_t entry = 0;
    long rr = resolve_user_path(user_path_ptr, path, sizeof(path));
    if (rr != 0) {
        return rr;
    }
    if (cndyn_load_package_for_exec(path, &entry) != 0) {
        return -ENOENT;
    }
    regs->rip = entry;
    regs->rsp = USER_EXEC_STACK_TOP;
    regs->rax = 0;
    return 0;
}

long user_sys_waitpid(long target_pid, uint64_t user_status_ptr) {
    proc_t *cur = process_current_user_or_kernel();
    uint64_t got_pid = 0;
    int got_status = 0;
    if (!cur) {
        return -EINVAL;
    }
    if (target_pid < -1) {
        return -EINVAL;
    }
    if (process_reap_child(cur, target_pid <= 0 ? 0 : (uint64_t)target_pid, &got_pid, &got_status) == 0) {
        if (user_status_ptr != 0) {
            if (!vmm_user_range_writable(user_status_ptr, sizeof(int))) {
                return -EFAULT;
            }
            *(volatile int *)(uintptr_t)user_status_ptr = got_status;
        }
        return (long)got_pid;
    }
    cur->wait_target = (target_pid <= 0) ? 0 : (uint64_t)target_pid;
    cur->state = PROC_STATE_BLOCKED;
    while (cur->state == PROC_STATE_BLOCKED) {
        sched_yield();
    }
    if (cur->wait_result_pid == 0) {
        return -EINVAL;
    }
    got_pid = cur->wait_result_pid;
    got_status = cur->wait_result_status;
    cur->wait_result_pid = 0;
    cur->wait_result_status = 0;
    (void)process_reap_child(cur, got_pid, NULL, NULL);
    if (user_status_ptr != 0) {
        if (!vmm_user_range_writable(user_status_ptr, sizeof(int))) {
            return -EFAULT;
        }
        *(volatile int *)(uintptr_t)user_status_ptr = got_status;
    }
    return (long)got_pid;
}

long user_sys__exit(long code, struct registers *regs) {
    proc_t *cur = process_current_user_or_kernel();
    process_mark_exit(cur, (int)code);
    user_on_syscall_exit(regs);
    return 0;
}

long user_sys_fork(struct registers *regs) {
    proc_t *cur = process_current_user_or_kernel();
    proc_t *child;
    if (!cur) {
        return -EINVAL;
    }
    child = process_alloc_user(cur->pid);
    if (!child) {
        return -EAGAIN;
    }
    child->pml4_phys = cur->pml4_phys;
    child->vm_flags = 1u; /* CHASEROS_VM_COW */
    child->user_entry = regs->rip;
    child->user_stack_top = regs->rsp;
    /*
     * COW 第一版占位：登记共享地址空间关系，后续由 vmm_cow fault 路径补全写时复制。
     * 目前子进程先不调度执行，返回给父进程子 pid。
     */
    return (long)child->pid;
}

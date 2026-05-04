/* kernel/process.c - 进程管理与调度实现（RR 策略见 sched.c） */

#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>

extern uint64_t chaseros_active_user_pid;

uint64_t g_sched_context_switches = 0;

static scheduler_t sched;

static void process_clear_runtime(proc_t *p) {
    p->ppid = 0;
    p->exit_code = 0;
    p->exited = 0;
    p->wait_target = 0;
    p->wait_result_pid = 0;
    p->wait_result_status = 0;
    p->user_entry = 0;
    p->user_stack_top = 0;
    p->vm_flags = 0;
    p->open_files = NULL;
    p->sender_queue = NULL;
    p->next_in_queue = NULL;
    p->msg_pending = 0;
}

proc_t *process_tasks(void) {
    return sched.tasks;
}

uint32_t process_sched_ready_count(void) {
    unsigned n = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (sched.tasks[i].state == PROC_STATE_READY) {
            n++;
        }
    }
    return (uint32_t)n;
}

proc_t *get_current_process(void) {
    return &sched.tasks[sched.current_task_idx];
}

void process_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        sched.tasks[i].state = PROC_STATE_FREE;
        process_clear_runtime(&sched.tasks[i]);
    }

    proc_t *kernel_proc = &sched.tasks[0];
    kernel_proc->pid = 0;
    kernel_proc->state = PROC_STATE_RUNNING;
    kernel_proc->pml4_phys = vmm_get_current_pml4();
    process_clear_runtime(kernel_proc);

    sched.current_task_idx = 0;
    sched.task_count = 1;
}

proc_t *process_find_by_pid(uint64_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (sched.tasks[i].state != PROC_STATE_FREE && sched.tasks[i].pid == pid) {
            return &sched.tasks[i];
        }
    }
    return NULL;
}

static void init_kernel_stack(proc_t *p, void (*entry)(void)) {
    p->stack_base = (uint8_t *)pmm_alloc_page();
    uint64_t *stack_top = (uint64_t *)(p->stack_base + 4096);

    *(--stack_top) = 0x10;
    uint64_t *rsp_ptr = stack_top;
    *(--stack_top) = (uint64_t)rsp_ptr;
    *(--stack_top) = 0x202;
    *(--stack_top) = 0x08;
    *(--stack_top) = (uint64_t)entry;
    *(--stack_top) = 0;
    *(--stack_top) = 0;

    for (int j = 0; j < 15; j++) {
        *(--stack_top) = 0;
    }

    p->rsp = (uint64_t)stack_top;
    p->pml4_phys = vmm_get_current_pml4();
}

proc_t *process_alloc_user(uint64_t ppid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (sched.tasks[i].state != PROC_STATE_FREE) {
            continue;
        }
        proc_t *p = &sched.tasks[i];
        p->pid = (uint64_t)i;
        p->ppid = ppid;
        p->state = PROC_STATE_READY;
        p->pml4_phys = vmm_get_current_pml4();
        process_clear_runtime(p);
        p->pid = (uint64_t)i;
        p->ppid = ppid;
        if (i + 1 > sched.task_count) {
            sched.task_count = i + 1;
        }
        return p;
    }
    return NULL;
}

proc_t *process_create(void (*entry)(void)) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (sched.tasks[i].state == PROC_STATE_FREE) {
            proc_t *p = &sched.tasks[i];
            p->pid = (uint64_t)i;
            p->ppid = 0;
            p->state = PROC_STATE_READY;
            process_clear_runtime(p);
            p->pid = (uint64_t)i;

            init_kernel_stack(p, entry);

            sched.task_count++;
            return p;
        }
    }
    return NULL;
}

proc_t *process_spawn_kernel_at_pid(uint64_t pid, void (*entry)(void)) {
    int idx = (int)pid;
    if (idx <= 0 || idx >= MAX_TASKS || entry == NULL) {
        return NULL;
    }
    proc_t *p = &sched.tasks[idx];
    if (p->state != PROC_STATE_FREE) {
        return NULL;
    }

    p->pid = pid;
    p->ppid = 0;
    p->state = PROC_STATE_READY;
    process_clear_runtime(p);
    p->pid = pid;

    init_kernel_stack(p, entry);

    if (idx + 1 > sched.task_count) {
        sched.task_count = idx + 1;
    }
    return p;
}

void process_bind_user_slot(uint64_t pid) {
    process_bind_user_slot_ex(pid, 0);
}

void process_bind_user_slot_ex(uint64_t pid, uint64_t ppid) {
    int idx = (int)pid;
    if (idx <= 0 || idx >= MAX_TASKS) {
        return;
    }
    proc_t *p = &sched.tasks[idx];
    if (p->state != PROC_STATE_FREE && p->pid != pid) {
        return;
    }
    p->pid = pid;
    p->ppid = ppid;
    p->state = PROC_STATE_USER_SLOT;
    p->pml4_phys = vmm_get_current_pml4();
    p->stack_base = NULL;
    p->rsp = 0;
    process_clear_runtime(p);
    p->pid = pid;
    p->ppid = ppid;
    p->state = PROC_STATE_USER_SLOT;
    p->pml4_phys = vmm_get_current_pml4();
    if (idx + 1 > sched.task_count) {
        sched.task_count = idx + 1;
    }
}

void process_mark_exit(proc_t *p, int status) {
    if (!p) {
        return;
    }
    p->exit_code = status;
    p->exited = 1;
    p->state = PROC_STATE_EXITED;
    if (p->pid == chaseros_active_user_pid) {
        chaseros_active_user_pid = 0;
    }
    proc_t *parent = process_find_by_pid(p->ppid);
    if (parent && parent->state == PROC_STATE_BLOCKED &&
        (parent->wait_target == 0 || parent->wait_target == p->pid)) {
        parent->wait_result_pid = p->pid;
        parent->wait_result_status = status;
        parent->state = PROC_STATE_READY;
    }
}

int process_reap_child(proc_t *parent, uint64_t target_pid, uint64_t *out_pid, int *out_status) {
    if (!parent) {
        return -1;
    }
    for (int i = 0; i < MAX_TASKS; i++) {
        proc_t *p = &sched.tasks[i];
        if (p->state == PROC_STATE_FREE) {
            continue;
        }
        if (p->ppid != parent->pid) {
            continue;
        }
        if (target_pid != 0 && p->pid != target_pid) {
            continue;
        }
        if (!p->exited) {
            continue;
        }
        if (out_pid) {
            *out_pid = p->pid;
        }
        if (out_status) {
            *out_status = p->exit_code;
        }
        if (p->stack_base) {
            pmm_free_page(p->stack_base);
        }
        p->state = PROC_STATE_FREE;
        process_clear_runtime(p);
        return 0;
    }
    return -1;
}

proc_t *process_current_task_for_ipc(void) {
    proc_t *u = process_find_by_pid(chaseros_active_user_pid);
    /*
     * ipc_send 成功交付后用户会变为 WAITING_REPLY；若在 USER_SLOT 之外就退回
     * get_current_process()（多为 shell 所在内核线程），则 SYS_IPC_CALL 里
     * ipc_wait_until_pending 会等错 PCB，表现为 hello/cnrun/slime 在首条 IPC 后死等。
     */
    if (u && chaseros_active_user_pid != 0) {
        if (u->state == PROC_STATE_USER_SLOT || u->state == PROC_STATE_WAITING_REPLY ||
            u->state == PROC_STATE_SENDING) {
            return u;
        }
    }
    return get_current_process();
}

void process_ipc_wake_from_receive(proc_t *p) {
    if (p->state != PROC_STATE_RECEIVING) {
        return;
    }
    if (p->pid == chaseros_active_user_pid && chaseros_active_user_pid != 0) {
        p->state = PROC_STATE_USER_SLOT;
    } else {
        p->state = PROC_STATE_READY;
    }
}

void process_ipc_wake_after_reply(proc_t *peer) {
    if (peer->state != PROC_STATE_WAITING_REPLY) {
        return;
    }
    if (peer->pid == chaseros_active_user_pid && chaseros_active_user_pid != 0) {
        peer->state = PROC_STATE_USER_SLOT;
    } else {
        peer->state = PROC_STATE_READY;
    }
}

void schedule(void) {
    int old_idx = sched.current_task_idx;
    int next_idx = (old_idx + 1) % MAX_TASKS;

    while (sched.tasks[next_idx].state != PROC_STATE_READY && next_idx != old_idx) {
        next_idx = (next_idx + 1) % MAX_TASKS;
    }

    if (sched.tasks[next_idx].state == PROC_STATE_READY) {
        if (sched.tasks[old_idx].state == PROC_STATE_RUNNING) {
            sched.tasks[old_idx].state = PROC_STATE_READY;
        }
        sched.tasks[next_idx].state = PROC_STATE_RUNNING;
        sched.current_task_idx = next_idx;
        g_sched_context_switches++;
    }
}

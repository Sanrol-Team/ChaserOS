/* kernel/user_fd.c — 用户态 open/close/read；阶段 3 可选经 IPC 服务委派 */

#include "user_fd.h"
#include "fs/vfs.h"
#include "errno.h"
#include "syscall_abi.h"
#include "vmm.h"
#ifdef CNOS_HYBRID_FS_VIA_IPC
#include "hybrid_ipc.h"
#endif

#include <stdint.h>

#define CNOS_USER_FD_MIN 3
#define CNOS_USER_FD_NUM 8

typedef struct {
    int valid;
    char path[128];
    uint32_t offset;
    uint32_t size;
} user_fd_slot_t;

static user_fd_slot_t g_ufd[CNOS_USER_FD_NUM];

void user_fd_reset(void) {
    for (int i = 0; i < CNOS_USER_FD_NUM; i++) {
        g_ufd[i].valid = 0;
    }
}

void user_fd_umount_close_all(void) {
    user_fd_reset();
}

static long copy_user_path(char *dst, size_t dst_sz, uint64_t ua) {
    size_t i;
    for (i = 0; i < dst_sz - 1u; i++) {
        if (!vmm_user_range_readable(ua + i, 1)) {
            return -EFAULT;
        }
        char c = *(const volatile char *)(uintptr_t)(ua + i);
        dst[i] = c;
        if (c == '\0') {
            return 0;
        }
    }
    dst[dst_sz - 1u] = '\0';
    return -EINVAL;
}

long user_fd_open_kpath(const char *path, int flags) {
    (void)flags;
    if (!vfs_is_mounted()) {
        return -EINVAL;
    }
    vfs_stat_t st;
    int vs = vfs_stat(path, &st);
    if (vs == VFS_ERR_NOTMOUNTED) {
        return -EINVAL;
    }
    if (vs != VFS_ERR_NONE) {
        return -ENOENT;
    }
    int idx = -1;
    for (int i = 0; i < CNOS_USER_FD_NUM; i++) {
        if (!g_ufd[i].valid) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        return -EMFILE;
    }

    size_t pi = 0;
    while (path[pi] && pi + 1 < sizeof(g_ufd[idx].path)) {
        g_ufd[idx].path[pi] = path[pi];
        pi++;
    }
    g_ufd[idx].path[pi] = '\0';

    g_ufd[idx].valid = 1;
    g_ufd[idx].offset = 0;
    if (st.size > 0xFFFFFFFFull) {
        g_ufd[idx].size = 0xFFFFFFFFu;
    } else {
        g_ufd[idx].size = (uint32_t)st.size;
    }
    return (long)(CNOS_USER_FD_MIN + idx);
}

long user_fd_sys_open(uint64_t path_ptr, int flags) {
    char path[128];

    if (!vfs_is_mounted()) {
        return -EINVAL;
    }
    long ce = copy_user_path(path, sizeof(path), path_ptr);
    if (ce != 0) {
        return ce;
    }

#ifdef CNOS_HYBRID_FS_VIA_IPC
    return hybrid_user_fd_open_via_ipc(path, flags);
#else
    return user_fd_open_kpath(path, flags);
#endif
}

long user_fd_sys_close(int fd) {
    if (fd < CNOS_USER_FD_MIN || fd >= CNOS_USER_FD_MIN + CNOS_USER_FD_NUM) {
        return -EBADF;
    }
    int idx = fd - CNOS_USER_FD_MIN;
    if (!g_ufd[idx].valid) {
        return -EBADF;
    }
    g_ufd[idx].valid = 0;
    return 0;
}

long user_fd_sys_read(int fd, uint64_t ua, size_t len) {
    if (fd < CNOS_USER_FD_MIN || fd >= CNOS_USER_FD_MIN + CNOS_USER_FD_NUM) {
        return -EBADF;
    }
    int idx = fd - CNOS_USER_FD_MIN;
    if (!g_ufd[idx].valid) {
        return -EBADF;
    }
    if (len > CNOS_SYSCALL_MAX_IO_BYTES) {
        return -EINVAL;
    }
    if (len == 0) {
        return 0;
    }
    if (!vmm_user_range_writable(ua, len)) {
        return -EFAULT;
    }

    uint32_t off = g_ufd[idx].offset;
    uint32_t sz = g_ufd[idx].size;
    if (off >= sz) {
        return 0;
    }

    size_t remain = (size_t)(sz - off);
    if (len > remain) {
        len = remain;
    }

    size_t total = 0;
    uint8_t kchunk[2048];

    while (total < len) {
        size_t chunk = len - total;
        if (chunk > sizeof(kchunk)) {
            chunk = sizeof(kchunk);
        }
        size_t got = 0;
        int vr = vfs_read_file_range(g_ufd[idx].path, off + (uint32_t)total, kchunk, chunk, &got);
        if (vr != VFS_ERR_NONE) {
            return -EIO;
        }
        if (got == 0) {
            break;
        }
        size_t i;
        for (i = 0; i < got; i++) {
            *(volatile char *)(uintptr_t)(ua + total + i) = (char)kchunk[i];
        }
        total += got;
        if (got < chunk) {
            break;
        }
    }

    g_ufd[idx].offset = off + (uint32_t)total;
    return (long)total;
}

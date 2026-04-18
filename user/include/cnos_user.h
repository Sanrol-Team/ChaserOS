/* cnos_user.h — CNOS 裸机用户 ELF 公共 ABI（freestanding）
 *
 * 约定与 kernel/syscall_abi.h、user/SYSCALL-ABI.txt、integrations/slime-for-cnos/std 一致。
 * 链接脚本：user/user.ld；入口符号：_start
 */

#ifndef CNOS_USER_H
#define CNOS_USER_H

#include <stddef.h>

/* ---- 调用号（须与内核 CNOS_SYS_* 一致） -------------------------------- */
#define CNOS_SYS_EXIT     0
#define CNOS_SYS_WRITE    1
#define CNOS_MAX_WRITE_LEN (1024u * 1024u)

/* ---- errno（正数；若系统调用返回负值，则 errno = -返回值） ------------ */
#define CNOS_EPERM      1
#define CNOS_EBADF      9
#define CNOS_EINVAL     22
#define CNOS_EFAULT     14
#define CNOS_ENOSYS     38

/**
 * SYS_WRITE：返回值在 RAX（≥0 为字节数；<0 为 -errno）。
 */
static inline long cnos_syscall_write(int fd, const void *buf, size_t len)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_WRITE),
          "D"((long)fd),
          "S"(buf),
          "d"(len)
        : "memory", "rcx", "r11");
    return ret;
}

/**
 * SYS_EXIT：不返回用户态；code 置于 RDI（内核可选用）。
 */
static inline void cnos_syscall_exit(int code)
{
    __asm__ volatile(
        "int $0x80"
        :
        : "a"((long)CNOS_SYS_EXIT), "D"((long)code)
        : "memory");
    __builtin_unreachable();
}

#endif /* CNOS_USER_H */

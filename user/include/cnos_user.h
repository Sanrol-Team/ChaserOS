/* cnos_user.h — CNOS 裸机用户 ELF 公共 ABI */

#ifndef CNOS_USER_H
#define CNOS_USER_H

#include <stddef.h>
#include <stdint.h>

#define CNOS_SYS_EXIT           0
#define CNOS_SYS_WRITE          1
#define CNOS_SYS_GETPID         2
#define CNOS_SYS_READ           3
#define CNOS_SYS_UPTIME_TICKS   4
#define CNOS_SYS_OPEN           5
#define CNOS_SYS_CLOSE          6
#define CNOS_SYS_IPC_SEND       7
#define CNOS_SYS_IPC_RECV       8
#define CNOS_SYS_IPC_REPLY      9
#define CNOS_SYS_IPC_CALL       10

#define CNOS_MAX_IO_LEN (1024u * 1024u)
#define CNOS_MAX_WRITE_LEN CNOS_MAX_IO_LEN
#define CNOS_MAX_MSG_PAYLOAD    48u

#define CNOS_HYBRID_SERVICE_PID   3ull
#define CNOS_MSG_NOP              0ull
#define CNOS_MSG_PING             1ull
#define CNOS_MSG_PONG             2ull
#define CNOS_MSG_FS_OPEN          10ull
#define CNOS_MSG_FS_REPLY         11ull

#define CNOS_EPERM      1
#define CNOS_ENOENT     2
#define CNOS_EIO        5
#define CNOS_EBADF      9
#define CNOS_EMFILE     24
#define CNOS_EINVAL     22
#define CNOS_EFAULT     14
#define CNOS_ENOSYS     38
#define CNOS_EAGAIN     11

typedef struct {
    uint64_t sender;
    uint64_t type;
    uint8_t payload[CNOS_MAX_MSG_PAYLOAD];
} cnos_message_t;

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

static inline long cnos_syscall_read(int fd, void *buf, size_t len)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_READ),
          "D"((long)fd),
          "S"(buf),
          "d"(len)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_getpid(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_GETPID)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_uptime_ticks(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_UPTIME_TICKS)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_open(const char *path, int flags)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_OPEN),
          "D"(path),
          "S"((long)flags)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_close(int fd)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_CLOSE),
          "D"((long)fd)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_ipc_send(uint64_t dest_pid, cnos_message_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_IPC_SEND),
          "D"(dest_pid),
          "S"(msg)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_ipc_recv(uint64_t src_pid, cnos_message_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_IPC_RECV),
          "D"(src_pid),
          "S"(msg)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long cnos_syscall_ipc_reply(uint64_t dest_pid, const cnos_message_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_IPC_REPLY),
          "D"(dest_pid),
          "S"(msg)
        : "memory", "rcx", "r11");
    return ret;
}

/** 原子请求-响应：RDI=dest，RSI=请求，RDX=响应（与内核一致） */
static inline long cnos_syscall_ipc_call(uint64_t dest_pid, const cnos_message_t *req, cnos_message_t *rep)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CNOS_SYS_IPC_CALL),
          "D"(dest_pid),
          "S"(req),
          "d"(rep)
        : "memory", "rcx", "r11");
    return ret;
}

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

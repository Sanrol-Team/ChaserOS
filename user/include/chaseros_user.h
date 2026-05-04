/* chaseros_user.h — ChaserOS 裸机用户 ELF 公共 ABI */

#ifndef CHASEROS_USER_H
#define CHASEROS_USER_H

#include <stddef.h>
#include <stdint.h>

#define CHASEROS_SYS_EXIT           0
#define CHASEROS_SYS_WRITE          1
#define CHASEROS_SYS_GETPID         2
#define CHASEROS_SYS_READ           3
#define CHASEROS_SYS_UPTIME_TICKS   4
#define CHASEROS_SYS_OPEN           5
#define CHASEROS_SYS_CLOSE          6
#define CHASEROS_SYS_IPC_SEND       7
#define CHASEROS_SYS_IPC_RECV       8
#define CHASEROS_SYS_IPC_REPLY      9
#define CHASEROS_SYS_IPC_CALL       10
#define CHASEROS_SYS_SPAWN          14
#define CHASEROS_SYS_EXEC           15
#define CHASEROS_SYS_WAITPID        16
#define CHASEROS_SYS__EXIT          17
#define CHASEROS_SYS_FORK           18

#define CHASEROS_MAX_IO_LEN (1024u * 1024u)
#define CHASEROS_MAX_WRITE_LEN CHASEROS_MAX_IO_LEN
#define CHASEROS_MAX_MSG_PAYLOAD    48u

#define CHASEROS_HYBRID_SERVICE_PID   3ull
#define CHASEROS_MSG_NOP              0ull
#define CHASEROS_MSG_PING             1ull
#define CHASEROS_MSG_PONG             2ull
#define CHASEROS_MSG_FS_OPEN          10ull
#define CHASEROS_MSG_FS_REPLY         11ull

#define CHASEROS_EPERM      1
#define CHASEROS_ENOENT     2
#define CHASEROS_EIO        5
#define CHASEROS_EBADF      9
#define CHASEROS_EMFILE     24
#define CHASEROS_EINVAL     22
#define CHASEROS_EFAULT     14
#define CHASEROS_ENOSYS     38
#define CHASEROS_EAGAIN     11

typedef struct {
    uint64_t sender;
    uint64_t type;
    uint8_t payload[CHASEROS_MAX_MSG_PAYLOAD];
} chaseros_message_t;

static inline long chaseros_syscall_write(int fd, const void *buf, size_t len)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_WRITE),
          "D"((long)fd),
          "S"(buf),
          "d"(len)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_read(int fd, void *buf, size_t len)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_READ),
          "D"((long)fd),
          "S"(buf),
          "d"(len)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_getpid(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_GETPID)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_uptime_ticks(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_UPTIME_TICKS)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_open(const char *path, int flags)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_OPEN),
          "D"(path),
          "S"((long)flags)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_close(int fd)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_CLOSE),
          "D"((long)fd)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_ipc_send(uint64_t dest_pid, chaseros_message_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_IPC_SEND),
          "D"(dest_pid),
          "S"(msg)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_ipc_recv(uint64_t src_pid, chaseros_message_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_IPC_RECV),
          "D"(src_pid),
          "S"(msg)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_ipc_reply(uint64_t dest_pid, const chaseros_message_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_IPC_REPLY),
          "D"(dest_pid),
          "S"(msg)
        : "memory", "rcx", "r11");
    return ret;
}

/** 原子请求-响应：RDI=dest，RSI=请求，RDX=响应（与内核一致） */
static inline long chaseros_syscall_ipc_call(uint64_t dest_pid, const chaseros_message_t *req, chaseros_message_t *rep)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_IPC_CALL),
          "D"(dest_pid),
          "S"(req),
          "d"(rep)
        : "memory", "rcx", "r11");
    return ret;
}

static inline void chaseros_syscall_exit(int code)
{
    __asm__ volatile(
        "int $0x80"
        :
        : "a"((long)CHASEROS_SYS_EXIT), "D"((long)code)
        : "memory");
    __builtin_unreachable();
}

static inline long chaseros_syscall_spawn(const char *path)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_SPAWN), "D"(path)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_exec(const char *path)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_EXEC), "D"(path)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_waitpid(long pid, int *status)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_WAITPID), "D"(pid), "S"(status)
        : "memory", "rcx", "r11");
    return ret;
}

static inline long chaseros_syscall_fork(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((long)CHASEROS_SYS_FORK)
        : "memory", "rcx", "r11");
    return ret;
}

#endif /* CHASEROS_USER_H */

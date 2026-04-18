/* user/hello.c - 独立 ELF，由内核以嵌入二进制装入 0x400000 */

#include <stdint.h>
#include "cnos_user.h"

static void write_cstr(int fd, const char *s)
{
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    (void)cnos_syscall_write(fd, s, n);
}

static void write_u64_dec(int fd, unsigned long long v)
{
    char buf[24];
    int i = 0;
    if (v == 0) {
        (void)cnos_syscall_write(fd, "0", 1);
        return;
    }
    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (v % 10ull));
        v /= 10ull;
    }
    while (i > 0) {
        char c = buf[--i];
        (void)cnos_syscall_write(fd, &c, 1);
    }
}

static void write_s64_dec(int fd, long v)
{
    if (v < 0) {
        (void)cnos_syscall_write(fd, "-", 1);
        write_u64_dec(fd, (unsigned long long)(-(unsigned long)v));
    } else {
        write_u64_dec(fd, (unsigned long long)v);
    }
}

void _start(void)
{
    char scratch[8];

    write_cstr(1, "Hello from user mode (ring 3)\n");
    write_cstr(1, "pid=");
    write_u64_dec(1, (unsigned long long)cnos_syscall_getpid());
    write_cstr(1, " uptime_ticks=");
    write_u64_dec(1, (unsigned long long)cnos_syscall_uptime_ticks());
    write_cstr(1, "\n");

    /* 阶段 3：IPC_CALL 一次 ping/pong（PID=3 服务） */
    {
        cnos_message_t req;
        cnos_message_t rep;
        req.sender = 0;
        req.type = CNOS_MSG_PING;
        for (size_t i = 0; i < CNOS_MAX_MSG_PAYLOAD; i++) {
            req.payload[i] = 0;
        }
        req.payload[0] = (uint8_t)'x';
        long cr = cnos_syscall_ipc_call(CNOS_HYBRID_SERVICE_PID, &req, &rep);
        write_cstr(1, "ipc_call(ping)=");
        write_s64_dec(1, cr);
        write_cstr(1, " pong_type=");
        write_u64_dec(1, (unsigned long long)rep.type);
        write_cstr(1, "\n");
    }

    long n = cnos_syscall_read(0, scratch, sizeof(scratch));
    write_cstr(1, "read(stdin) returned ");
    if (n >= 0) {
        write_u64_dec(1, (unsigned long long)n);
    } else {
        write_cstr(1, "errno=");
        write_u64_dec(1, (unsigned long long)(-n));
    }
    write_cstr(1, "\n");

    /* VFS 已挂载时：打开根目录下文件名；否则多为 EINVAL（未挂载）或 ENOENT */
    static const char bogus[] = "__cnos_no_such_file__";
    long fd = cnos_syscall_open(bogus, 0);
    write_cstr(1, "open(bogus)=");
    write_s64_dec(1, fd);
    write_cstr(1, "\n");

    cnos_syscall_exit(0);
}

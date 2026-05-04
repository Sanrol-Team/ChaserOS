#include <stddef.h>
#include <stdint.h>

#define CHASEROS_SYS_EXIT 0
#define CHASEROS_SYS_WRITE 1
#define CHASEROS_SYS_READ 3
#define CHASEROS_SYS_GETPID 2

static long chaseros_syscall3(long n, long a0, long a1, long a2) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(n), "D"(a0), "S"(a1), "d"(a2)
        : "memory", "rcx", "r11");
    return ret;
}

long chaseros_write(int fd, const void *buf, size_t len) {
    return chaseros_syscall3(CHASEROS_SYS_WRITE, fd, (long)(uintptr_t)buf, (long)len);
}

long chaseros_read(int fd, void *buf, size_t len) {
    return chaseros_syscall3(CHASEROS_SYS_READ, fd, (long)(uintptr_t)buf, (long)len);
}

long chaseros_getpid(void) {
    return chaseros_syscall3(CHASEROS_SYS_GETPID, 0, 0, 0);
}

void chaseros_exit(int code) {
    (void)chaseros_syscall3(CHASEROS_SYS_EXIT, code, 0, 0);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

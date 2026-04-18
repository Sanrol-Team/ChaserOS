/* user/hello.c - 独立 ELF，由内核以嵌入二进制装入 0x400000 */

#include "cnos_user.h"

void _start(void)
{
    static const char msg[] = "Hello from user mode (ring 3)\n";
    (void)cnos_syscall_write(1, msg, sizeof(msg) - 1);
    cnos_syscall_exit(0);
}

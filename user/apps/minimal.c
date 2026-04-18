/* 最小用户 ELF 模板 — 逻辑需与 cnos_user.h / SYSCALL-ABI.txt 一致 */

#include "cnos_user.h"

void _start(void)
{
    static const char msg[] = "CNOS user minimal\n";
    (void)cnos_syscall_write(1, msg, sizeof(msg) - 1);
    cnos_syscall_exit(0);
}

/* syscall_abi.h — CNOS 用户态系统调用编号与寄存器角色（单一事实来源）
 *
 * 陷阱：int $0x80（向量 128），仅当 CS.CPL=3 时在 isr.c 中按本文档处理。
 */

#ifndef KERNEL_SYSCALL_ABI_H
#define KERNEL_SYSCALL_ABI_H

#define CNOS_SYSCALL_VECTOR 0x80u

#define CNOS_SYS_EXIT           0u
#define CNOS_SYS_WRITE          1u
#define CNOS_SYS_GETPID         2u
#define CNOS_SYS_READ           3u
#define CNOS_SYS_UPTIME_TICKS   4u
#define CNOS_SYS_OPEN           5u
#define CNOS_SYS_CLOSE          6u
#define CNOS_SYS_IPC_SEND       7u
#define CNOS_SYS_IPC_RECV       8u
#define CNOS_SYS_IPC_REPLY      9u
#define CNOS_SYS_IPC_CALL       10u

#define CNOS_SYSCALL_MAX_IO_BYTES (1024u * 1024u)
#define CNOS_SYSCALL_MAX_WRITE_BYTES CNOS_SYSCALL_MAX_IO_BYTES

/* IPC_CALL: RDI=dest_pid, RSI=请求 message_t*, RDX=响应 message_t* */

#endif /* KERNEL_SYSCALL_ABI_H */

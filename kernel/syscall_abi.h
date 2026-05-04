/* syscall_abi.h — ChaserOS 用户态系统调用编号与寄存器角色（单一事实来源）
 *
 * 陷阱：int $0x80（向量 128），仅当 CS.CPL=3 时在 isr.c 中按本文档处理。
 */

#ifndef KERNEL_SYSCALL_ABI_H
#define KERNEL_SYSCALL_ABI_H

#define CHASEROS_SYSCALL_VECTOR 0x80u

#define CHASEROS_SYS_EXIT           0u
#define CHASEROS_SYS_WRITE          1u
#define CHASEROS_SYS_GETPID         2u
#define CHASEROS_SYS_READ           3u
#define CHASEROS_SYS_UPTIME_TICKS   4u
#define CHASEROS_SYS_OPEN           5u
#define CHASEROS_SYS_CLOSE          6u
#define CHASEROS_SYS_IPC_SEND       7u
#define CHASEROS_SYS_IPC_RECV       8u
#define CHASEROS_SYS_IPC_REPLY      9u
#define CHASEROS_SYS_IPC_CALL       10u
#define CHASEROS_SYS_DLOPEN         11u
#define CHASEROS_SYS_DLSYM          12u
#define CHASEROS_SYS_DLCLOSE        13u
#define CHASEROS_SYS_SPAWN          14u
#define CHASEROS_SYS_EXEC           15u
#define CHASEROS_SYS_WAITPID        16u
#define CHASEROS_SYS__EXIT          17u
#define CHASEROS_SYS_FORK           18u

#define CHASEROS_SYSCALL_MAX_IO_BYTES (1024u * 1024u)
#define CHASEROS_SYSCALL_MAX_WRITE_BYTES CHASEROS_SYSCALL_MAX_IO_BYTES

/* IPC_CALL: RDI=dest_pid, RSI=请求 message_t*, RDX=响应 message_t* */

#endif /* KERNEL_SYSCALL_ABI_H */

/* kernel/ipc.h - 微内核进程间通信 (IPC) */

#ifndef IPC_H
#define IPC_H

#include <stdint.h>

/* 
 * 微内核 IPC 设计：同步消息传递 (Synchronous Message Passing)
 * 参考 Minix/QNX 的设计。
 */

#define MAX_MSG_PAYLOAD 48 /* 每个消息携带的有效载荷大小 (字节) */

/* 消息结构 */
typedef struct {
    uint64_t sender;   /* 发送者进程 ID */
    uint64_t type;     /* 消息类型 */
    uint8_t payload[MAX_MSG_PAYLOAD]; /* 消息内容 */
} message_t;

/* IPC 错误码 */
#define IPC_OK       0
#define IPC_ERROR   -1

/* 
 * 内核 IPC 核心函数
 * 这些函数通常由系统调用触发。
 */

/* 向目标进程发送消息并等待回复 (Send-Receive) */
int ipc_send(uint64_t dest_pid, message_t *msg);

/* 接收来自特定或任意进程的消息 */
int ipc_receive(uint64_t src_pid, message_t *msg);

/* 回复消息 (不阻塞) */
int ipc_reply(uint64_t dest_pid, message_t *msg);

#endif

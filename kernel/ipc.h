/* kernel/ipc.h - 进程间通信 */

#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#define MAX_MSG_PAYLOAD 48

typedef struct {
    uint64_t sender;
    uint64_t type;
    uint8_t payload[MAX_MSG_PAYLOAD];
} message_t;

#define IPC_OK           0
#define IPC_ERROR       -1
#define IPC_WOULD_BLOCK -2

struct proc;

int ipc_send(uint64_t dest_pid, message_t *msg);
int ipc_receive(uint64_t src_pid, message_t *msg);
int ipc_reply(uint64_t dest_pid, message_t *msg);

void ipc_wait_until_pending(struct proc *cur);
void ipc_consume_pending_message(struct proc *cur, message_t *out);

#endif

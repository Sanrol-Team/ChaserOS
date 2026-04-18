/* kernel/ipc.c - IPC 实现（Send / Receive / Reply + 阻塞等待） */

#include "ipc.h"
#include "process.h"
#include <stdint.h>
#include <stddef.h>

static void msg_copy_to_buffer(proc_t *dest, proc_t *src, const message_t *msg) {
    dest->msg_buffer.sender = src->pid;
    dest->msg_buffer.type = msg->type;
    uint8_t *s = (uint8_t *)msg->payload;
    uint8_t *d = (uint8_t *)dest->msg_buffer.payload;
    for (int i = 0; i < MAX_MSG_PAYLOAD; i++) {
        d[i] = s[i];
    }
}

static void msg_copy_out(message_t *out, const proc_t *from) {
    out->sender = from->msg_buffer.sender;
    out->type = from->msg_buffer.type;
    uint8_t *s = (uint8_t *)from->msg_buffer.payload;
    uint8_t *d = (uint8_t *)out->payload;
    for (int i = 0; i < MAX_MSG_PAYLOAD; i++) {
        d[i] = s[i];
    }
}

static void stash_outgoing(proc_t *sender, const message_t *msg) {
    msg_copy_to_buffer(sender, sender, msg);
}

static void sender_enqueue(proc_t *dest, proc_t *sender) {
    sender->next_in_queue = dest->sender_queue;
    dest->sender_queue = sender;
}

static proc_t *sender_dequeue(proc_t *recv, uint64_t src_pid) {
    proc_t **pp = &recv->sender_queue;
    while (*pp) {
        proc_t *s = *pp;
        if (src_pid == 0 || s->pid == src_pid) {
            *pp = s->next_in_queue;
            s->next_in_queue = NULL;
            return s;
        }
        pp = &s->next_in_queue;
    }
    return NULL;
}

void ipc_wait_until_pending(struct proc *cur) {
    proc_t *p = cur;
    while (!p->msg_pending) {
        __asm__ volatile("sti\n\thlt\n\tcli" ::: "memory");
    }
}

void ipc_consume_pending_message(struct proc *cur, message_t *out) {
    proc_t *p = cur;
    msg_copy_out(out, p);
    p->msg_pending = 0;
}

int ipc_send(uint64_t dest_pid, message_t *msg) {
    if (!msg) {
        return IPC_ERROR;
    }

    proc_t *current = process_current_task_for_ipc();
    proc_t *dest = process_find_by_pid(dest_pid);

    if (!dest || dest->state == PROC_STATE_FREE) {
        return IPC_ERROR;
    }
    if (dest == current) {
        return IPC_ERROR;
    }

    if (dest->state == PROC_STATE_RECEIVING) {
        msg_copy_to_buffer(dest, current, msg);
        dest->msg_pending = 1;
        process_ipc_wake_from_receive(dest);
        return IPC_OK;
    }

    stash_outgoing(current, msg);
    current->state = PROC_STATE_SENDING;
    sender_enqueue(dest, current);
    return IPC_OK;
}

int ipc_receive(uint64_t src_pid, message_t *msg) {
    if (!msg) {
        return IPC_ERROR;
    }

    proc_t *current = process_current_task_for_ipc();

    if (current->msg_pending &&
        (src_pid == 0 || current->msg_buffer.sender == src_pid)) {
        msg_copy_out(msg, current);
        current->msg_pending = 0;
        process_ipc_wake_from_receive(current);
        return IPC_OK;
    }

    proc_t *sender = sender_dequeue(current, src_pid);
    if (sender) {
        msg_copy_out(msg, sender);
        sender->state = PROC_STATE_WAITING_REPLY;
        return IPC_OK;
    }

    current->state = PROC_STATE_RECEIVING;
    return IPC_WOULD_BLOCK;
}

int ipc_reply(uint64_t dest_pid, message_t *msg) {
    if (!msg) {
        return IPC_ERROR;
    }

    proc_t *current = process_current_task_for_ipc();
    proc_t *peer = process_find_by_pid(dest_pid);

    if (!peer || peer->state != PROC_STATE_WAITING_REPLY) {
        return IPC_ERROR;
    }

    msg_copy_to_buffer(peer, current, msg);
    peer->msg_pending = 1;
    process_ipc_wake_after_reply(peer);
    return IPC_OK;
}

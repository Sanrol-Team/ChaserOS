/* hybrid_ipc.c — PID=3 内核服务：PING/PONG + FS_OPEN（委托 user_fd_open_kpath） */

#include "hybrid_ipc.h"
#include "ipc.h"
#include "process.h"
#include "user_fd.h"
#include "errno.h"

#include <stddef.h>

static int64_t unpack_i64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= (uint64_t)p[i] << (i * 8);
    }
    return (int64_t)v;
}

static void pack_i64(uint8_t *p, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) {
        p[i] = (uint8_t)((u >> (i * 8)) & 0xFFu);
    }
}

long hybrid_user_fd_open_via_ipc(const char *path, int flags) {
    (void)flags;
    message_t req;
    req.sender = 0;
    req.type = CHASEROS_MSG_FS_OPEN;
    size_t i = 0;
    for (; path[i] && i + 1 < (size_t)MAX_MSG_PAYLOAD; i++) {
        req.payload[i] = (uint8_t)path[i];
    }
    req.payload[i] = 0;
    while (i < (size_t)MAX_MSG_PAYLOAD) {
        req.payload[i++] = 0;
    }

    if (ipc_send(CHASEROS_HYBRID_SERVICE_PID, &req) != IPC_OK) {
        return -EIO;
    }

    proc_t *cur = process_current_task_for_ipc();
    ipc_wait_until_pending((struct proc *)cur);

    message_t rep;
    ipc_consume_pending_message((struct proc *)cur, &rep);
    if (rep.type != CHASEROS_MSG_FS_REPLY) {
        return -EIO;
    }
    return (long)unpack_i64(rep.payload);
}

static void chaseros_hybrid_ipc_service_loop(void) {
    for (;;) {
        message_t m;
        if (ipc_receive(0, &m) != IPC_OK) {
            continue;
        }

        if (m.type == CHASEROS_MSG_FS_OPEN) {
            char path[MAX_MSG_PAYLOAD];
            size_t j = 0;
            for (; j < sizeof(path) - 1; j++) {
                path[j] = (char)m.payload[j];
                if (path[j] == '\0') {
                    break;
                }
            }
            path[sizeof(path) - 1] = '\0';

            long fd = user_fd_open_kpath(path, 0);
            message_t reply;
            reply.sender = CHASEROS_HYBRID_SERVICE_PID;
            reply.type = CHASEROS_MSG_FS_REPLY;
            for (int k = 0; k < MAX_MSG_PAYLOAD; k++) {
                reply.payload[k] = 0;
            }
            pack_i64(reply.payload, (int64_t)fd);
            (void)ipc_reply(m.sender, &reply);
            continue;
        }

        message_t reply;
        reply.sender = CHASEROS_HYBRID_SERVICE_PID;
        for (int i = 0; i < MAX_MSG_PAYLOAD; i++) {
            reply.payload[i] = m.payload[i];
        }
        reply.type = (m.type == CHASEROS_MSG_PING) ? CHASEROS_MSG_PONG : CHASEROS_MSG_NOP;
        (void)ipc_reply(m.sender, &reply);
    }
}

void chaseros_hybrid_ipc_service_spawn(void) {
    (void)process_spawn_kernel_at_pid(CHASEROS_HYBRID_SERVICE_PID, chaseros_hybrid_ipc_service_loop);
}

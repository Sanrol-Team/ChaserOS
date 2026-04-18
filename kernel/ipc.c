/* kernel/ipc.c - 微内核进程间通信 (IPC) 实现 */

#include "ipc.h"
#include "process.h"
#include <stdint.h>
#include <stddef.h>

/* 
 * 同步 IPC 核心逻辑：
 * - 发送 (Send)：如果接收者正在等待接收 (Receiving)，则直接将消息拷贝给接收者并唤醒它。
 *             否则，将发送者加入接收者的等待队列 (Sender Queue) 并阻塞发送者。
 * - 接收 (Receive)：如果等待队列中有发送者，则从队列头部取出一个发送者的消息并唤醒它。
 *               否则，将当前接收者设为等待接收状态 (Receiving) 并阻塞它。
 */

/* 辅助函数：将数据从发送者进程拷贝到接收者进程的消息缓冲区 */
static void msg_copy(proc_t *src, proc_t *dest, message_t *msg) {
    /* 简单的结构体内存拷贝 */
    dest->msg_buffer.sender = src->pid;
    dest->msg_buffer.type = msg->type;
    uint8_t *s = (uint8_t *)msg->payload;
    uint8_t *d = (uint8_t *)dest->msg_buffer.payload;
    for (int i = 0; i < MAX_MSG_PAYLOAD; i++) d[i] = s[i];
}

int ipc_send(uint64_t dest_pid, message_t *msg) {
    proc_t *current = get_current_process();
    /* 此处需要根据 dest_pid 找到对应的进程控制块 (待实现进程表查询) */
    proc_t *dest = NULL; /* TODO: find_proc_by_pid(dest_pid) */
    
    if (!dest) return IPC_ERROR;

    if (dest->state == PROC_STATE_RECEIVING) {
        /* 接收者正在等待消息，直接进行零拷贝/内存拷贝并唤醒接收者 */
        msg_copy(current, dest, msg);
        dest->state = PROC_STATE_READY;
        /* TODO: 触发调度器切换 (唤醒目标进程) */
    } else {
        /* 接收者忙碌，将发送者阻塞并加入接收者的等待队列 */
        current->state = PROC_STATE_SENDING;
        /* TODO: 加入 dest->sender_queue 链表头部或尾部 */
        /* TODO: 触发调度器切换 (让出 CPU) */
    }
    
    return IPC_OK;
}

int ipc_receive(uint64_t src_pid, message_t *msg) {
    proc_t *current = get_current_process();
    
    if (current->sender_queue) {
        /* 如果有进程已经在等待发送消息给当前进程 */
        /* TODO: 从 current->sender_queue 中取出第一个发送者 */
        proc_t *sender = current->sender_queue;
        /* TODO: 从发送者的 msg_buffer 拷贝消息到当前进程的缓冲区 */
        sender->state = PROC_STATE_READY;
        /* TODO: 触发调度器切换 (唤醒发送者) */
    } else {
        /* 没有进程在等待，阻塞当前接收进程 */
        current->state = PROC_STATE_RECEIVING;
        /* TODO: 触发调度器切换 (让出 CPU) */
    }
    
    return IPC_OK;
}

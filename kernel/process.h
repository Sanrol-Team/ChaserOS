/* kernel/process.h - 进程/线程控制块 (PCB/TCB) */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "ipc.h"

#define MAX_TASKS 16

/* 进程状态定义 */
typedef enum {
    PROC_STATE_FREE,        /* 槽位未使用 */
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_SENDING,
    PROC_STATE_RECEIVING,
    PROC_STATE_SLEEPING,
    PROC_STATE_EXITED
} proc_state_t;

/*
 * 与 interrupts.asm 中 [rax+16] 保存 rsp 的布局一致：
 * pid(8) + state(4) + 填充(4) -> rsp 在偏移 16
 */
typedef struct proc {
    uint64_t pid;
    proc_state_t state;
    uint64_t rsp;               /* 偏移 16：当前保存的内核栈指针 */
    uint8_t *stack_base;        /* pmm 分配的内核栈页 */
    uint64_t pml4_phys;

    message_t msg_buffer;
    struct proc *next_in_queue;
    struct proc *sender_queue;
} proc_t;

typedef struct {
    proc_t tasks[MAX_TASKS];
    int current_task_idx;
    int task_count;
} scheduler_t;

proc_t *get_current_process(void);
void process_init(void);
proc_t *process_create(void (*entry)(void));
void schedule(void);

#endif

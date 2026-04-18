/* kernel/process.h - 进程/线程控制块 (PCB/TCB) */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "ipc.h"

#define MAX_TASKS 16

typedef enum {
    PROC_STATE_FREE,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_SENDING,
    PROC_STATE_RECEIVING,
    PROC_STATE_SLEEPING,
    PROC_STATE_EXITED,
    PROC_STATE_WAITING_REPLY,
    PROC_STATE_USER_SLOT
} proc_state_t;

typedef struct proc {
    uint64_t pid;
    proc_state_t state;
    uint64_t rsp;
    uint8_t *stack_base;
    uint64_t pml4_phys;

    message_t msg_buffer;
    struct proc *next_in_queue;
    struct proc *sender_queue;
    int msg_pending;
} proc_t;

typedef struct {
    proc_t tasks[MAX_TASKS];
    int current_task_idx;
    int task_count;
} scheduler_t;

proc_t *get_current_process(void);
proc_t *process_find_by_pid(uint64_t pid);
void process_init(void);
proc_t *process_create(void (*entry)(void));
proc_t *process_spawn_kernel_at_pid(uint64_t pid, void (*entry)(void));

void process_bind_user_slot(uint64_t pid);
proc_t *process_current_task_for_ipc(void);
void process_ipc_wake_from_receive(proc_t *p);
void process_ipc_wake_after_reply(proc_t *peer);

void schedule(void);

#endif

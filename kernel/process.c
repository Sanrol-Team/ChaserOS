/* kernel/process.c - 进程管理与调度实现 */

#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include <stddef.h>

static scheduler_t sched;

proc_t *get_current_process() {
    return &sched.tasks[sched.current_task_idx];
}

void process_init() {
    for (int i = 0; i < MAX_TASKS; i++) {
        sched.tasks[i].state = PROC_STATE_FREE;
    }

    /* 初始化内核主进程 (PID 0) */
    proc_t *kernel_proc = &sched.tasks[0];
    kernel_proc->pid = 0;
    kernel_proc->state = PROC_STATE_RUNNING;
    kernel_proc->pml4_phys = vmm_get_current_pml4();
    
    sched.current_task_idx = 0;
    sched.task_count = 1;
}

proc_t *process_create(void (*entry)()) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (sched.tasks[i].state == PROC_STATE_FREE) {
            proc_t *p = &sched.tasks[i];
            p->pid = (uint64_t)i;
            p->state = PROC_STATE_READY;
            
            /* 分配内核栈 */
            p->stack_base = (uint8_t *)pmm_alloc_page();
            uint64_t *stack_top = (uint64_t *)(p->stack_base + 4096);
            
            /* 初始化栈上的上下文 (需与 isr_handler 结构一致) */
            /* CPU 自动压栈部分 */
            *(--stack_top) = 0x10;              /* SS */
            uint64_t *rsp_ptr = stack_top;      /* 暂存 RSP 位置 */
            *(--stack_top) = (uint64_t)rsp_ptr; /* RSP */
            *(--stack_top) = 0x202;             /* RFLAGS */
            *(--stack_top) = 0x08;              /* CS */
            *(--stack_top) = (uint64_t)entry;   /* RIP */
            
            /* 模拟错误代码和中断号 */
            *(--stack_top) = 0;                 /* err_code */
            *(--stack_top) = 0;                 /* int_no */
            
            /* 通用寄存器 (rax to r15) */
            for (int j = 0; j < 15; j++) {
                *(--stack_top) = 0;
            }
            
            p->rsp = (uint64_t)stack_top;
            p->pml4_phys = vmm_get_current_pml4();
            
            sched.task_count++;
            return p;
        }
    }
    return NULL;
}

void schedule() {
    int old_idx = sched.current_task_idx;
    int next_idx = (old_idx + 1) % MAX_TASKS;

    while (sched.tasks[next_idx].state != PROC_STATE_READY && next_idx != old_idx) {
        next_idx = (next_idx + 1) % MAX_TASKS;
    }

    if (sched.tasks[next_idx].state == PROC_STATE_READY) {
        sched.tasks[old_idx].state = PROC_STATE_READY;
        sched.tasks[next_idx].state = PROC_STATE_RUNNING;
        sched.current_task_idx = next_idx;
    }
}

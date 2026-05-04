/* kernel/sched.c - 混合内核调度入口与观测 */

#include "sched.h"
#include "process.h"

#include <stddef.h>

extern void puts(const char *s);
extern void puts_dec(uint64_t n);

extern uint64_t g_sched_context_switches;

void sched_init(void) {
    g_sched_context_switches = 0;
}

void sched_on_timer_tick(void) {
    schedule();
}

void sched_yield(void) {
    schedule();
}

uint64_t sched_context_switches(void) {
    return g_sched_context_switches;
}

uint32_t sched_ready_kernel_threads(void) {
    return process_sched_ready_count();
}

static const char *sched_state_name(proc_state_t s) {
    switch (s) {
    case PROC_STATE_FREE:
        return "free";
    case PROC_STATE_READY:
        return "ready";
    case PROC_STATE_RUNNING:
        return "run";
    case PROC_STATE_BLOCKED:
        return "blocked";
    case PROC_STATE_SENDING:
        return "sending";
    case PROC_STATE_RECEIVING:
        return "rcv";
    case PROC_STATE_SLEEPING:
        return "sleep";
    case PROC_STATE_EXITED:
        return "exit";
    case PROC_STATE_WAITING_REPLY:
        return "waitreply";
    case PROC_STATE_USER_SLOT:
        return "user";
    default:
        return "?";
    }
}

static const char *sched_pid_role(uint64_t pid) {
    if (pid == CHASEROS_SCHED_PID_IDLE) {
        return "idle";
    }
    if (pid == CHASEROS_SCHED_PID_USER) {
        return "ring3";
    }
    if (pid == CHASEROS_SCHED_PID_IPC_FS) {
        return "ipc-fs";
    }
    return "kthread";
}

void sched_print_task_table(void) {
    proc_t *tasks = process_tasks();
    puts("Hybrid scheduler (IRQ0 RR; USER_SLOT not in RR)\n");
    puts("  ctx_switches=");
    puts_dec(sched_context_switches());
    puts("  ready_kernel_threads=");
    puts_dec((uint64_t)sched_ready_kernel_threads());
    puts("\n");
    puts("slot  pid  state        role\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == PROC_STATE_FREE) {
            continue;
        }
        puts_dec((uint64_t)(unsigned)i);
        puts("     ");
        puts_dec(tasks[i].pid);
        puts("  ");
        puts(sched_state_name(tasks[i].state));
        puts("  ");
        puts(sched_pid_role(tasks[i].pid));
        puts("\n");
    }
}

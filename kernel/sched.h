/* kernel/sched.h - 混合内核调度框架（非「纯微内核」模型）
 *
 * ChaserOS 采用 **混合内核** 结构：
 *   - **单体部分**：驱动、内存管理、VFS/ext2、控制台等运行在同一内核地址空间；
 *   - **内核线程**：固定槽位上的协作式轮转（IRQ0 触发 `sched_on_timer_tick`），用于 IPC 服务等；
 *   - **用户槽位**：ring 3 任务以 `USER_SLOT` 表示，不参与 RR，由 `int $0x80` / `iret` 驱动；
 *   - **IPC 与调度**：SENDING / RECEIVING / WAITING_REPLY 等状态与 `sched_yield` 协作。
 *
 * 因此调度器是「填充后的」显式框架，而非把一切服务外迁的纯微内核。
 */

#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include <stdint.h>

/** 固定 PID 约定（与 process_spawn_kernel_at_pid / user 绑定一致） */
#define CHASEROS_SCHED_PID_IDLE   0ull
#define CHASEROS_SCHED_PID_USER   2ull
#define CHASEROS_SCHED_PID_IPC_FS 3ull

void sched_init(void);

/** IRQ0：内核 tick 里调用，驱动协作式 RR */
void sched_on_timer_tick(void);

/** IPC 等路径显式让出 CPU（与内核 `schedule()` 等价） */
void sched_yield(void);

uint64_t sched_context_switches(void);
uint32_t sched_ready_kernel_threads(void);

/** Shell `ps`：打印当前任务槽与状态 */
void sched_print_task_table(void);

#endif

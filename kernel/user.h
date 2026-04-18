#ifndef KERNEL_USER_H
#define KERNEL_USER_H

#include <stddef.h>
#include <stdint.h>
#include "isr.h"

/** ring 3 嵌入式任务 PID（跳入用户前由 user.c 设置） */
extern uint64_t cnos_active_user_pid;

void user_run_embedded_hello(void);

/** 嵌入的 Slime（hello.sm）用户 ELF；未启用 CNOS_HAVE_SLIME_USER 时为空实现 */
void user_run_embedded_slime_hello(void);

/** 解析内核内嵌的首个 CNAF（MANIFEST + hello.elf IMAGE），演示 cnaf → ELF 装载 */
void user_run_embedded_demo_cnaf(void);

/**
 * 解析暂存于物理连续页中的 .cnaf，装入 IMAGE 并跳入 ring 3。
 * 成功后不返回；失败时释放 blob 并打印诊断。
 */
void user_run_cnaf_from_owned_pages(uint8_t *blob, size_t blob_sz, uint64_t blob_pages,
                                    const char *tag);

void user_on_syscall_exit(struct registers *regs);

void userland_exit_done(void);

#endif

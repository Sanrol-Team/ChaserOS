#ifndef KERNEL_USER_H
#define KERNEL_USER_H

#include "isr.h"

void user_run_embedded_hello(void);

/** 嵌入的 Slime（hello.sm）用户 ELF；未启用 CNOS_HAVE_SLIME_USER 时为空实现 */
void user_run_embedded_slime_hello(void);

void user_on_syscall_exit(struct registers *regs);

void userland_exit_done(void);

#endif

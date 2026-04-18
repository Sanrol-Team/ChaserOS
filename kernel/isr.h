/* kernel/isr.h - 与 interrupts.asm / isr.c 共享的中断帧布局 */

#ifndef ISR_H
#define ISR_H

#include <stdint.h>

struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

#endif

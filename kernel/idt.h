/* kernel/idt.h - 中断描述符表 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* IDT 条目结构 */
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

/* IDTR 寄存器结构 */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* 初始化 IDT */
void idt_init();

/* 设置 IDT 条目 */
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);

#endif
    
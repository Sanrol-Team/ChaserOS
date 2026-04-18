/* kernel/gdt.h - 长模式 GDT 与 TSS（用户态 Ring 3） */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

#endif

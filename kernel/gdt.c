/* kernel/gdt.c - GDT、TSS（Ring 0→3 中断时 RSP0） */

#include "gdt.h"
#include "pmm.h"
#include <stddef.h>
#include <stdint.h>

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss64 tss;

static uint64_t gdt[7];

extern void gdt_load(struct gdt_ptr *ptr);
extern void load_tss(void);

static void encode_tss_descriptor(uint64_t *low, uint64_t *high, uint64_t base, uint32_t limit) {
    *low = (uint64_t)(limit & 0xFFFFu) |
        ((uint64_t)(base & 0xFFFFULL) << 16) |
        ((uint64_t)((base >> 16) & 0xFFULL) << 32) |
        ((uint64_t)0x89ULL << 40) |
        ((uint64_t)((limit >> 16) & 0x0FULL) << 48) |
        ((uint64_t)((base >> 24) & 0xFFULL) << 56);
    *high = (base >> 32) & 0xFFFFFFFFULL;
}

void gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x0020980000000000ULL;
    gdt[2] = 0x0000920000000000ULL;
    gdt[3] = 0x0020FA0000000000ULL;
    gdt[4] = 0x0000F20000000000ULL;

    uint8_t *istack = (uint8_t *)pmm_alloc_page();
    tss.rsp0 = (uint64_t)(istack + 4096);
    tss.iomap_base = sizeof(struct tss64);

    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = (uint32_t)sizeof(struct tss64) - 1u;
    encode_tss_descriptor(&gdt[5], &gdt[6], tss_base, tss_limit);

    struct gdt_ptr gp;
    gp.limit = (uint16_t)(sizeof(gdt) - 1u);
    gp.base = (uint64_t)&gdt;
    gdt_load(&gp);
    load_tss();
}

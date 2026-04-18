/* 简单 bump 分配器：使用 linker.ld 中的 .fs_reserve，供 libext2fs 接入 malloc 前使用 */

#include "fs/cnos/porting.h"

extern char _fs_reserve_start[];
extern char _fs_reserve_end[];

void *cnos_malloc(size_t n) {
    static uintptr_t bump;

    if (n == 0) {
        n = 1;
    }
    n = (n + 7u) & ~7u;

    uintptr_t base = (uintptr_t)_fs_reserve_start + bump;
    uintptr_t end = base + n;
    if (end > (uintptr_t)_fs_reserve_end) {
        return NULL;
    }
    bump += n;
    return (void *)base;
}

void cnos_free(void *p) {
    (void)p;
}

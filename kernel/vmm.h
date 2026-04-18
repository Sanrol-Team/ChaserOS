/* kernel/vmm.h - 虚拟内存管理器 */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include "pmm.h"

/* 分页相关的位定义 */
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)
#define PAGE_USER    (1ULL << 2)
#define PAGE_NX      (1ULL << 63)

/* 获取当前 PML4 地址 */
uint64_t vmm_get_current_pml4();

/* 初始化虚拟内存管理器 */
void vmm_init();

/* 映射虚拟地址到物理地址 */
void vmm_map(uint64_t pml4, uint64_t virt, uint64_t phys, uint64_t flags);

/* 取消映射虚拟地址 */
void vmm_unmap(uint64_t pml4, uint64_t virt);

/* 创建新的地址空间 (PML4) */
uint64_t vmm_create_address_space();

/*
 * 在 2MB 大页（entry.asm 引导页表）上为含 virt 的槽位置 U/S，使用户态可访问该 2MiB 区域。
 * 完整 4KiB 粒度隔离需日后替换引导页表实现。
 */
void vmm_grant_user_2mb_region(uint64_t virt);

/**
 * [addr, addr+len) 是否在 **当前** 页表中均为用户可读页（PAGE_PRESENT | PAGE_USER）。
 * len==0 时视作合法（不产生访问）。
 */
int vmm_user_range_readable(uint64_t addr, size_t len);

#endif

/* kernel/pmm.h - 物理内存管理器 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

/* 初始化物理内存管理器 (mbi_phys: Multiboot2 信息结构物理地址) */
void pmm_init(uint64_t mbi_phys);

/* 分配一个物理页 */
void *pmm_alloc_page();

/* 分配 n 个连续物理页（地址连续）；失败返回 NULL */
void *pmm_alloc_contiguous_pages(uint64_t n);

/* 释放 pmm_alloc_contiguous_pages 分配的连续区域 */
void pmm_free_contiguous(void *ptr, uint64_t n);

/* 释放一个物理页 */
void pmm_free_page(void *ptr);

/* 获取总内存大小 (字节) */
uint64_t pmm_get_total_memory();

/* 获取已用内存大小 (字节) */
uint64_t pmm_get_used_memory();

/* 获取空闲内存大小 (字节) */
uint64_t pmm_get_free_memory();

#endif

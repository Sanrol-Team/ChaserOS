/* kernel/pmm.c - 物理内存管理器实现 */

#include "pmm.h"
#include "multiboot.h"
#include <stdint.h>
#include <stddef.h>

extern char _kernel_start[];
extern char _kernel_end[]; /* 由链接脚本定义 */

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t used_pages;
static uint64_t bitmap_size;

/* 辅助函数：设置位图中特定页的状态 */
static inline void bitmap_set(uint64_t page_index) {
    bitmap[page_index / 8] |= (1 << (page_index % 8));
}

/* 辅助函数：清除位图中特定页的状态 */
static inline void bitmap_clear(uint64_t page_index) {
    bitmap[page_index / 8] &= ~(1 << (page_index % 8));
}

/* 辅助函数：测试位图中特定页的状态 (1=已用, 0=空闲) */
static inline int bitmap_test(uint64_t page_index) {
    return (bitmap[page_index / 8] >> (page_index % 8)) & 1;
}

static struct multiboot_tag *pmm_next_tag(struct multiboot_tag *tag) {
    uint32_t sz = tag->size;
    uint8_t *p = (uint8_t *)tag + ((sz + 7U) & ~7U);
    return (struct multiboot_tag *)p;
}

void pmm_init(uint64_t mbi_phys) {
    struct multiboot_tag_mmap *mmap_tag = NULL;
    struct multiboot_tag *tag =
        (struct multiboot_tag *)((uint8_t *)(uintptr_t)mbi_phys + 8);

    for (;;) {
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            mmap_tag = (struct multiboot_tag_mmap *)tag;
            break;
        }
        tag = pmm_next_tag(tag);
    }

    uint64_t max_addr = 0;
    if (mmap_tag && mmap_tag->size >= sizeof(struct multiboot_tag_mmap) &&
        mmap_tag->entry_size >= sizeof(struct multiboot_mmap_entry)) {
        uint8_t *p = (uint8_t *)mmap_tag + sizeof(struct multiboot_tag_mmap);
        uint8_t *end = (uint8_t *)mmap_tag + mmap_tag->size;
        while (p + mmap_tag->entry_size <= end) {
            struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint64_t end_addr = e->base + e->length;
                if (end_addr > max_addr) {
                    max_addr = end_addr;
                }
            }
            p += mmap_tag->entry_size;
        }
    }

    total_pages = max_addr / PAGE_SIZE;
    if (total_pages == 0) {
        total_pages = 1;
    }
    bitmap_size = total_pages / 8;
    if (total_pages % 8 != 0) bitmap_size++;

    /* 将位图放在内核结束后的位置 */
    bitmap = (uint8_t *)_kernel_end;
    
    /* 初始时将所有内存标记为已用 (不可用) */
    for (uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    used_pages = total_pages;

    if (mmap_tag && mmap_tag->size >= sizeof(struct multiboot_tag_mmap) &&
        mmap_tag->entry_size >= sizeof(struct multiboot_mmap_entry)) {
        uint8_t *p = (uint8_t *)mmap_tag + sizeof(struct multiboot_tag_mmap);
        uint8_t *end = (uint8_t *)mmap_tag + mmap_tag->size;
        while (p + mmap_tag->entry_size <= end) {
            struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint64_t start_page = e->base / PAGE_SIZE;
                uint64_t end_page = (e->base + e->length) / PAGE_SIZE;
                for (uint64_t j = start_page; j < end_page; j++) {
                    bitmap_clear(j);
                    used_pages--;
                }
            }
            p += mmap_tag->entry_size;
        }
    }

    /* 将内核本身及位图占用的页面重新标记为已用 */
    uint64_t kernel_start_page = (uint64_t)_kernel_start / PAGE_SIZE;
    uint64_t kernel_end_page = ((uint64_t)_kernel_end + bitmap_size) / PAGE_SIZE + 1;
    for (uint64_t i = kernel_start_page; i < kernel_end_page; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
        }
    }
    
    /* 将前 1MB 标记为已用 (BIOS, VGA 等，包含 0x0 - 0x8000) */
    for (uint64_t i = 0; i < 0x100000 / PAGE_SIZE; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
        }
    }
}

void *pmm_alloc_page() {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            return (void *)(i * PAGE_SIZE);
        }
    }
    return NULL; /* 内存不足 */
}

void *pmm_alloc_contiguous_pages(uint64_t n) {
    if (n == 0) {
        return NULL;
    }
    for (uint64_t start = 0; start + n <= total_pages; start++) {
        int ok = 1;
        for (uint64_t j = 0; j < n; j++) {
            if (bitmap_test(start + j)) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            for (uint64_t j = 0; j < n; j++) {
                bitmap_set(start + j);
            }
            used_pages += n;
            return (void *)(start * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_contiguous(void *ptr, uint64_t n) {
    if (!ptr || n == 0) {
        return;
    }
    uint64_t page_index = (uint64_t)ptr / PAGE_SIZE;
    if (page_index >= total_pages) {
        return;
    }
    for (uint64_t j = 0; j < n && page_index + j < total_pages; j++) {
        if (bitmap_test(page_index + j)) {
            bitmap_clear(page_index + j);
            used_pages--;
        }
    }
}

void pmm_free_page(void *ptr) {
    uint64_t page_index = (uint64_t)ptr / PAGE_SIZE;
    if (page_index < total_pages && bitmap_test(page_index)) {
        bitmap_clear(page_index);
        used_pages--;
    }
}

uint64_t pmm_get_total_memory() {
    return total_pages * PAGE_SIZE;
}

uint64_t pmm_get_used_memory() {
    return used_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory() {
    return (total_pages - used_pages) * PAGE_SIZE;
}

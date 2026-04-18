/* kernel/multiboot.h - Multiboot2 引导信息 (GRUB2) */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

/* 帧缓冲信息（Multiboot2 tag 填充；与旧 graphics.h 布局一致，供日后恢复 GUI 时用） */
struct vbe_mode_info {
    uint16_t attributes;
    uint8_t window_a, window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a, segment_b;
    uint32_t win_func_ptr;
    uint32_t pitch;
    uint16_t width, height;
    uint8_t w_char, y_char, planes, bpp, banks;
    uint8_t memory_model, bank_size, image_pages;
    uint8_t reserved0;
    uint8_t red_mask, red_position;
    uint8_t green_mask, green_position;
    uint8_t blue_mask, blue_position;
    uint8_t reserved_mask, reserved_position;
    uint8_t direct_color_attributes;
    uint64_t framebuffer;
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
    uint8_t reserved1[206];
} __attribute__((packed));

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289U

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT_TAG_TYPE_MODULE      3

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved0;
    uint8_t reserved1;
} __attribute__((packed));

#define MULTIBOOT_MEMORY_AVAILABLE 1

struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    /* cmdline 紧跟其后，按 8 字节对齐填充 */
} __attribute__((packed));

int multiboot_validate(uint32_t magic, uint64_t mbi_phys);
int multiboot_fill_vbe(uint64_t mbi_phys, struct vbe_mode_info *out);

/* 查找 cmdline 以 suffix 结尾的模块（如 font.ttf）；返回物理起止地址 */
int multiboot_find_module_suffix(uint64_t mbi_phys, const char *suffix, uint32_t *mod_start, uint32_t *mod_end);

#endif

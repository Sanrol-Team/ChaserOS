/* kernel/multiboot.c - 解析 GRUB2 传递的 Multiboot2 信息 */

#include "multiboot.h"
#include <stddef.h>
#include <stdint.h>

static int suffix_match(const char *cmdline, const char *suffix) {
    size_t sl = 0;
    while (suffix[sl]) {
        sl++;
    }
    size_t i = 0;
    while (cmdline[i]) {
        i++;
    }
    if (i < sl) {
        return 0;
    }
    for (size_t j = 0; j < sl; j++) {
        if (cmdline[i - sl + j] != suffix[j]) {
            return 0;
        }
    }
    return 1;
}

static struct multiboot_tag *next_tag(struct multiboot_tag *tag) {
    uint32_t sz = tag->size;
    uint8_t *p = (uint8_t *)tag + ((sz + 7U) & ~7U);
    return (struct multiboot_tag *)p;
}

int multiboot_validate(uint32_t magic, uint64_t mbi_phys) {
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        return 0;
    }
    if (mbi_phys & 7ULL) {
        return 0;
    }
    uint32_t total = *(uint32_t *)(uintptr_t)mbi_phys;
    if (total < 16 || total > 0x100000) {
        return 0;
    }
    return 1;
}

int multiboot_fill_vbe(uint64_t mbi_phys, struct vbe_mode_info *out) {
    struct multiboot_tag *tag =
        (struct multiboot_tag *)((uint8_t *)(uintptr_t)mbi_phys + 8);
    for (;;) {
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER &&
            tag->size >= sizeof(struct multiboot_tag_framebuffer)) {
            struct multiboot_tag_framebuffer *fb =
                (struct multiboot_tag_framebuffer *)tag;
            if (fb->framebuffer_addr == 0 || fb->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
                tag = next_tag(tag);
                continue;
            }
            out->width = (uint16_t)fb->framebuffer_width;
            out->height = (uint16_t)fb->framebuffer_height;
            out->pitch = fb->framebuffer_pitch;
            out->bpp = fb->framebuffer_bpp;
            out->framebuffer = fb->framebuffer_addr;
            return 0;
        }
        tag = next_tag(tag);
    }
    return -1;
}

int multiboot_has_efi_handoff(uint64_t mbi_phys) {
    struct multiboot_tag *tag =
        (struct multiboot_tag *)((uint8_t *)(uintptr_t)mbi_phys + 8);
    for (;;) {
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        if (tag->type == MULTIBOOT_TAG_TYPE_EFI32 || tag->type == MULTIBOOT_TAG_TYPE_EFI64) {
            return 1;
        }
        tag = next_tag(tag);
    }
    return 0;
}

int multiboot_find_module_suffix(uint64_t mbi_phys, const char *suffix, uint32_t *mod_start, uint32_t *mod_end) {
    if (!suffix || !mod_start || !mod_end) {
        return -1;
    }
    struct multiboot_tag *tag =
        (struct multiboot_tag *)((uint8_t *)(uintptr_t)mbi_phys + 8);
    for (;;) {
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE && tag->size >= sizeof(struct multiboot_tag_module)) {
            struct multiboot_tag_module *m = (struct multiboot_tag_module *)tag;
            const char *cmdline = (const char *)m + sizeof(struct multiboot_tag_module);
            uint32_t cmd_max = tag->size - (uint32_t)sizeof(struct multiboot_tag_module);
            char buf[128];
            uint32_t ci = 0;
            while (ci < cmd_max && ci + 1 < sizeof(buf) && cmdline[ci]) {
                buf[ci] = cmdline[ci];
                ci++;
            }
            buf[ci] = '\0';
            if (suffix_match(buf, suffix)) {
                *mod_start = m->mod_start;
                *mod_end = m->mod_end;
                return 0;
            }
        }
        tag = next_tag(tag);
    }
    return -1;
}

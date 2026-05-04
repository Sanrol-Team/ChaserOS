/* CNIM loader: maps segments and applies relative relocations. */

#include "fs/cnaf/cnab.h"
#include <stddef.h>
#include <stdint.h>

static void mem_copy(uint8_t *d, const uint8_t *s, size_t n) {
    while (n--) {
        *d++ = *s++;
    }
}

static int cnim_apply_rela(const cnim_header_t *h, const uint8_t *base, size_t image_size) {
    if (h->rela_count == 0u) {
        return 0;
    }
    if (h->rela_table_offset + (uint64_t)h->rela_count * sizeof(cnim_rela_t) > image_size) {
        return -1;
    }
    const cnim_rela_t *rela = (const cnim_rela_t *)(const void *)(base + h->rela_table_offset);
    for (uint32_t i = 0; i < h->rela_count; i++) {
        if (rela[i].type != CNIM_RELOC_RELATIVE64) {
            return -1;
        }
        uint64_t *site = (uint64_t *)(uintptr_t)(h->image_base + rela[i].offset);
        *site = h->image_base + rela[i].addend;
    }
    return 0;
}

int cnim_load_flat(const void *image, size_t image_size, uint64_t *out_entry) {
    if (image_size < sizeof(cnim_header_t)) {
        return -1;
    }
    const uint8_t *p = (const uint8_t *)image;
    const cnim_header_t *h = (const cnim_header_t *)(const void *)p;
    if (h->magic != CNIM_MAGIC_U32) {
        return -1;
    }
    if (h->fmt_major != CNIM_FMT_MAJOR || h->fmt_minor > CNIM_FMT_MINOR) {
        return -1;
    }
    if (h->header_size != CNIM_HEADER_SIZE) {
        return -1;
    }
    if (h->segment_table_offset + (uint64_t)h->segment_count * sizeof(cnim_segment_t) > image_size) {
        return -1;
    }
    const cnim_segment_t *seg =
        (const cnim_segment_t *)(const void *)(p + (size_t)h->segment_table_offset);
    for (uint32_t i = 0; i < h->segment_count; i++) {
        if (seg[i].file_offset + seg[i].file_size > image_size || seg[i].file_size > seg[i].mem_size) {
            return -1;
        }
        uint8_t *dest = (uint8_t *)(uintptr_t)seg[i].virt_addr;
        mem_copy(dest, p + (size_t)seg[i].file_offset, (size_t)seg[i].file_size);
        for (uint64_t j = seg[i].file_size; j < seg[i].mem_size; j++) {
            dest[j] = 0;
        }
    }
    if (cnim_apply_rela(h, p, image_size) != 0) {
        return -1;
    }
    if (out_entry) {
        *out_entry = h->image_base + h->entry_rva;
    }
    return 0;
}

int cnim_load_streaming(cnim_stream_read_t read_fn, void *ctx, size_t image_size, uint64_t *out_entry) {
    uint8_t hdr[sizeof(cnim_header_t)];
    if (image_size < sizeof(hdr)) {
        return -1;
    }
    if (read_fn(ctx, 0, hdr, sizeof(hdr)) != 0) {
        return -1;
    }
    const cnim_header_t *h = (const cnim_header_t *)(const void *)hdr;
    if (h->magic != CNIM_MAGIC_U32) {
        return -1;
    }
    if (h->fmt_major != CNIM_FMT_MAJOR || h->fmt_minor > CNIM_FMT_MINOR) {
        return -1;
    }
    if (h->header_size != CNIM_HEADER_SIZE) {
        return -1;
    }
    if (h->segment_table_offset + (uint64_t)h->segment_count * sizeof(cnim_segment_t) > image_size) {
        return -1;
    }
    cnim_segment_t seg[16];
    if (h->segment_count > 16u) {
        return -1;
    }
    if (read_fn(ctx, h->segment_table_offset, seg, (size_t)h->segment_count * sizeof(cnim_segment_t)) != 0) {
        return -1;
    }
    for (uint32_t i = 0; i < h->segment_count; i++) {
        if (seg[i].file_offset + seg[i].file_size > image_size || seg[i].file_size > seg[i].mem_size) {
            return -1;
        }
        uint8_t *dest = (uint8_t *)(uintptr_t)seg[i].virt_addr;
        uint64_t remain = seg[i].file_size;
        uint64_t off = seg[i].file_offset;
        while (remain > 0u) {
            size_t chunk = (size_t)(remain > 2048ull ? 2048ull : remain);
            if (read_fn(ctx, off, dest, chunk) != 0) {
                return -1;
            }
            remain -= chunk;
            off += chunk;
            dest += chunk;
        }
        for (uint64_t j = seg[i].file_size; j < seg[i].mem_size; j++) {
            ((uint8_t *)(uintptr_t)seg[i].virt_addr)[j] = 0;
        }
    }
    if (h->rela_count != 0u) {
        return -1;
    }
    if (out_entry) {
        *out_entry = h->image_base + h->entry_rva;
    }
    return 0;
}

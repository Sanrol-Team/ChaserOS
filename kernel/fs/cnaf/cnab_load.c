/* CNAB 装载：将 CNAF IMAGE 节中的 CNAB 映像拷入用户虚拟地址（身份映射），不读取 ELF */

#include "fs/cnaf/cnab.h"
#include <stddef.h>
#include <stdint.h>

static void mem_copy(uint8_t *d, const uint8_t *s, size_t n) {
    while (n--) {
        *d++ = *s++;
    }
}

int cnab_load_flat(const void *image, size_t image_size, uint64_t *out_entry) {
    if (image_size < sizeof(cnab_header_t)) {
        return -1;
    }
    const uint8_t *p = (const uint8_t *)image;
    const cnab_header_t *h = (const cnab_header_t *)(void *)p;
    if (h->magic != CNAB_MAGIC_U32) {
        return -1;
    }
    if (h->fmt_major != CNAB_FMT_MAJOR || h->fmt_minor > CNAB_FMT_MINOR) {
        return -1;
    }
    if (h->header_size != CNAB_HEADER_SIZE) {
        return -1;
    }
    if ((uint64_t)h->header_size + h->payload_size > image_size) {
        return -1;
    }
    uint8_t *dest = (uint8_t *)(uintptr_t)h->load_base;
    mem_copy(dest, p + h->header_size, (size_t)h->payload_size);
    if (out_entry) {
        *out_entry = h->load_base + h->entry_rva;
    }
    return 0;
}

int cnab_load_streaming(cnab_stream_read_t read_fn, void *ctx, size_t image_size, uint64_t *out_entry) {
    uint8_t hdr[sizeof(cnab_header_t)];
    if (image_size < sizeof(hdr)) {
        return -1;
    }
    if (read_fn(ctx, 0, hdr, sizeof(hdr)) != 0) {
        return -1;
    }
    const cnab_header_t *h = (const cnab_header_t *)(void *)hdr;
    if (h->magic != CNAB_MAGIC_U32) {
        return -1;
    }
    if (h->fmt_major != CNAB_FMT_MAJOR || h->fmt_minor > CNAB_FMT_MINOR) {
        return -1;
    }
    if (h->header_size != CNAB_HEADER_SIZE) {
        return -1;
    }
    if ((uint64_t)h->header_size + h->payload_size > image_size) {
        return -1;
    }
    uint8_t *dest = (uint8_t *)(uintptr_t)h->load_base;
    uint64_t remain = h->payload_size;
    uint64_t fo = h->header_size;
    while (remain > 0u) {
        size_t chunk = (size_t)(remain > 2048ull ? 2048ull : remain);
        if (read_fn(ctx, fo, dest, chunk) != 0) {
            return -1;
        }
        dest += chunk;
        fo += chunk;
        remain -= chunk;
    }
    if (out_entry) {
        *out_entry = h->load_base + h->entry_rva;
    }
    return 0;
}

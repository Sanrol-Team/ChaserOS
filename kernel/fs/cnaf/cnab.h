/* CNAB — CNAF IMAGE 节载荷：原生机器码映像（内核不解析 ELF） */

#ifndef CHASEROS_CNAB_H
#define CHASEROS_CNAB_H

#include <stddef.h>
#include <stdint.h>

#define CNAB_MAGIC_U32 0x42414E43u /* 'C' 'N' 'A' 'B' 小端 */

#define CNAB_FMT_MAJOR 0u
#define CNAB_FMT_MINOR 1u

#define CNAB_HEADER_SIZE 64u

/*
 * 布局：[cnab_header][payload]
 * payload 为连续机器码/数据，自 load_base 起映射到用户地址空间（身份映射低地址）。
 * 入口：RIP = load_base + entry_rva（通常 entry_rva 指向 _start）。
 */
typedef struct cnab_header {
    uint32_t magic;
    uint16_t fmt_major;
    uint16_t fmt_minor;
    uint64_t load_base;
    uint64_t entry_rva;
    uint64_t payload_size; /* header 之后字节数 */
    uint32_t header_size;  /* 固定 CNAB_HEADER_SIZE */
    uint32_t flags;        /* 预留 0 */
    uint8_t reserved[24];  /* 对齐至 64 字节 */
} cnab_header_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(cnab_header_t) == 64, "cnab_header size");
#endif

typedef int (*cnab_stream_read_t)(void *ctx, uint64_t off_in_image, void *buf, size_t len);

int cnab_load_flat(const void *image, size_t image_size, uint64_t *out_entry);

int cnab_load_streaming(cnab_stream_read_t read_fn, void *ctx, size_t image_size, uint64_t *out_entry);

#endif

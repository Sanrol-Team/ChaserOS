#ifndef KERNEL_ELF_LOAD_H
#define KERNEL_ELF_LOAD_H

#include <stddef.h>
#include <stdint.h>

int elf_load_flat(const void *elf_data, size_t elf_size, uint64_t *out_entry);

/**
 * 从「ELF 映像内偏移」流式读入并装载 PT_LOAD（用于 CNAF IMAGE 不经由整段缓冲）。
 * read_fn：从 ELF 字节 off 起读 len 到 buf；成功返回 0，失败返回 -1。
 */
typedef int (*elf_stream_read_t)(void *ctx, uint64_t offset_in_elf, void *buf, size_t len);

int elf_load_flat_streaming(elf_stream_read_t read_fn, void *ctx, size_t elf_size,
                            uint64_t *out_entry);

#endif

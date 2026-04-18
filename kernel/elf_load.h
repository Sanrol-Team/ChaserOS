#ifndef KERNEL_ELF_LOAD_H
#define KERNEL_ELF_LOAD_H

#include <stddef.h>
#include <stdint.h>

int elf_load_flat(const void *elf_data, size_t elf_size, uint64_t *out_entry);

#endif

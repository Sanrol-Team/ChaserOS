/* kernel/elf_load.c - 将 ELF64 ET_EXEC 装入身份映射低地址（与引导 2MiB 大页一致） */

#include "elf.h"
#include <stddef.h>
#include <stdint.h>

static void mem_copy(uint8_t *d, const uint8_t *s, size_t n) {
    while (n--) {
        *d++ = *s++;
    }
}

static void mem_zero(uint8_t *d, size_t n) {
    while (n--) {
        *d++ = 0;
    }
}

int elf_load_flat(const void *elf_data, size_t elf_size, uint64_t *out_entry) {
    const uint8_t *p = (const uint8_t *)elf_data;
    if (elf_size < sizeof(elf64_ehdr_t)) {
        return -1;
    }
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)p;
    if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 || eh->e_ident[2] != ELFMAG2 ||
        eh->e_ident[3] != ELFMAG3) {
        return -1;
    }
    if (eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB) {
        return -1;
    }
    if (eh->e_type != ET_EXEC || eh->e_machine != EM_X86_64) {
        return -1;
    }
    if (eh->e_phoff + (uint64_t)eh->e_phnum * (uint64_t)eh->e_phentsize > elf_size) {
        return -1;
    }
    if (out_entry) {
        *out_entry = eh->e_entry;
    }
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const elf64_phdr_t *ph =
            (const elf64_phdr_t *)(p + eh->e_phoff + (uint64_t)i * (uint64_t)eh->e_phentsize);
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        if (ph->p_offset + ph->p_filesz > elf_size) {
            return -1;
        }
        uint8_t *dest = (uint8_t *)(uintptr_t)ph->p_vaddr;
        mem_copy(dest, p + ph->p_offset, (size_t)ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz) {
            mem_zero(dest + (size_t)ph->p_filesz, (size_t)(ph->p_memsz - ph->p_filesz));
        }
    }
    return 0;
}

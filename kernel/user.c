/* kernel/user.c - Ring 3 用户态跳转与嵌入的 hello 程序 */

#include "user.h"
#include "user_fd.h"
#include "elf_load.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "fs/cnaf/cnaf_image.h"

#include <stddef.h>
#include <stdint.h>

#define USER_TEXT_BASE   0x400000ULL
#define USER_STACK_TOP   0x801000ULL

static void grant_user_regions(void) {
    vmm_grant_user_2mb_region(USER_TEXT_BASE, 0);
    vmm_grant_user_2mb_region(USER_STACK_TOP - 4096ULL, 1);
    vmm_flush_tlb_all();
}

extern void puts(const char *s);

uint64_t cnos_active_user_pid = 2;

extern char _binary_hello_bin_start[];
extern char _binary_hello_bin_end[];

extern char _binary_demo_cnaf_bin_start[];
extern char _binary_demo_cnaf_bin_end[];

#ifdef CNOS_HAVE_SLIME_USER
extern char _binary_hello_sm_bin_start[];
extern char _binary_hello_sm_bin_end[];
#endif

static void user_jump_to_ring3(uint64_t rip, uint64_t rsp) {
    __asm__ volatile(
        "pushq $0x23\n"
        "pushq %0\n"
        "pushq $0x2\n"
        "pushq $0x1B\n"
        "pushq %1\n"
        "iretq\n"
        :
        : "r"(rsp), "r"(rip)
        : "memory");
}

void userland_exit_done(void) {
    puts("\n[user] exited.\nCNOS> ");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void user_on_syscall_exit(struct registers *regs) {
    static uint8_t *stk;
    if (!stk) {
        stk = (uint8_t *)pmm_alloc_page();
    }
    regs->rip = (uint64_t)userland_exit_done;
    regs->cs = 0x08;
    regs->ss = 0x10;
    regs->rsp = (uint64_t)(stk + 4096);
    regs->rflags = 0x202;
}

static void user_run_cnaf_inner(const uint8_t *blob, size_t blob_sz, const char *tag, uint8_t *free_blob,
                                uint64_t free_pages) {
    const uint8_t *img = NULL;
    size_t img_sz = 0;
    cnaf_err_t ce = cnaf_extract_image(blob, blob_sz, &img, &img_sz);
    if (ce != CNAF_OK) {
        if (free_blob) {
            pmm_free_contiguous(free_blob, free_pages);
        }
        puts("cnaf: bundle invalid ("); puts(tag); puts(")\n");
        return;
    }

    uint64_t entry = 0;
    grant_user_regions();

    int lr = elf_load_flat(img, img_sz, &entry);
    if (free_blob) {
        pmm_free_contiguous(free_blob, free_pages);
    }
    if (lr != 0) {
        puts("cnaf: ELF load failed ("); puts(tag); puts(")\n");
        return;
    }

    cnos_active_user_pid = 2;
    process_bind_user_slot(cnos_active_user_pid);
    user_fd_reset();
    puts("[kernel] jumping to user ("); puts(tag); puts(", CNAF IMAGE, ring 3)...\n");
    user_jump_to_ring3(entry, USER_STACK_TOP);
}

static void user_run_embedded_elf(const uint8_t *blob_start, const uint8_t *blob_end, const char *tag) {
    size_t sz = (size_t)(blob_end - blob_start);
    uint64_t entry = 0;

    grant_user_regions();

    if (elf_load_flat(blob_start, sz, &entry) != 0) {
        puts("user: embedded ELF load failed ("); puts(tag); puts(")\n");
        return;
    }

    cnos_active_user_pid = 2;
    process_bind_user_slot(cnos_active_user_pid);
    user_fd_reset();
    puts("[kernel] jumping to user ("); puts(tag); puts(", ring 3)...\n");
    user_jump_to_ring3(entry, USER_STACK_TOP);
}

void user_run_embedded_hello(void) {
    user_run_embedded_elf((const uint8_t *)_binary_hello_bin_start,
                         (const uint8_t *)_binary_hello_bin_end, "C hello");
}

void user_run_embedded_slime_hello(void) {
#ifdef CNOS_HAVE_SLIME_USER
    user_run_embedded_elf((const uint8_t *)_binary_hello_sm_bin_start,
                         (const uint8_t *)_binary_hello_sm_bin_end, "Slime hello");
#else
    puts("Slime user ELF not embedded. Reconfigure with -DCNOS_WITH_SLIME_USER=ON (needs slimec in PATH).\n");
#endif
}

void user_run_embedded_demo_cnaf(void) {
    const uint8_t *p = (const uint8_t *)_binary_demo_cnaf_bin_start;
    size_t n = (size_t)(_binary_demo_cnaf_bin_end - _binary_demo_cnaf_bin_start);
    user_run_cnaf_inner(p, n, "embedded demo.cnaf", NULL, 0);
}

void user_run_cnaf_from_owned_pages(uint8_t *blob, size_t blob_sz, uint64_t blob_pages, const char *tag) {
    user_run_cnaf_inner(blob, blob_sz, tag, blob, blob_pages);
}

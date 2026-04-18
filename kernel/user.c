/* kernel/user.c - Ring 3 用户态跳转与嵌入的 hello 程序 */

#include "user.h"
#include "elf_load.h"
#include "pmm.h"
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>

extern void puts(const char *s);

extern char _binary_hello_bin_start[];
extern char _binary_hello_bin_end[];

#ifdef CNOS_HAVE_SLIME_USER
extern char _binary_hello_sm_bin_start[];
extern char _binary_hello_sm_bin_end[];
#endif

#define USER_TEXT_BASE   0x400000ULL
#define USER_STACK_TOP   0x801000ULL

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

static void user_run_embedded_elf(const uint8_t *blob_start, const uint8_t *blob_end, const char *tag) {
    size_t sz = (size_t)(blob_end - blob_start);
    uint64_t entry = 0;

    vmm_grant_user_2mb_region(USER_TEXT_BASE);
    vmm_grant_user_2mb_region(USER_STACK_TOP - 4096ULL);

    if (elf_load_flat(blob_start, sz, &entry) != 0) {
        puts("user: embedded ELF load failed ("); puts(tag); puts(")\n");
        return;
    }

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

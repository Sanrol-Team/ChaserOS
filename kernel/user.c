/* kernel/user.c - Ring 3 user entry and package launch helpers. */

#include "user.h"
#include "shell.h"
#include "user_fd.h"
#include "elf_load.h"
#include "pmm.h"
#include "process.h"
#include "vmm.h"
#include "loader/cndyn_loader.h"
#include "io.h"

#include <stddef.h>
#include <stdint.h>

#define USER_TEXT_BASE   0x400000ULL
/*
 * AMD64 SysV ABI：进入函数时 RSP≡8 (mod 16)（等效于 call 压入 8 字节后）。
 * 若 RSP 初始为 16 对齐（如 0x801000），_start 首条 push rbp 后 RBP≡8 (mod 16)，
 * GCC 生成的 movaps [rbp-0x90] 等会对未 16 对齐地址触发 #GP(0xD)。
 */
#define USER_STACK_TOP   0x801008ULL

static void grant_user_regions(void) {
    vmm_grant_user_2mb_region(USER_TEXT_BASE, 0);
    vmm_grant_user_2mb_region(USER_STACK_TOP - 4096ULL, 1);
    vmm_flush_tlb_all();
}

extern void puts(const char *s);

uint64_t chaseros_active_user_pid = 2;

static char g_user_cwd[256] = "/";

void chaseros_user_cwd_reset(void) {
    g_user_cwd[0] = '/';
    g_user_cwd[1] = '\0';
}

void chaseros_user_set_cwd(const char *abs_norm_path) {
    if (!abs_norm_path) {
        chaseros_user_cwd_reset();
        return;
    }
    size_t i = 0;
    while (abs_norm_path[i] && i + 1 < sizeof(g_user_cwd)) {
        g_user_cwd[i] = abs_norm_path[i];
        i++;
    }
    g_user_cwd[i] = '\0';
}

const char *chaseros_user_get_cwd(void) {
    return g_user_cwd;
}

extern char _binary_hello_bin_start[];
extern char _binary_hello_bin_end[];

extern char _binary_demo_cnaf_bin_start[];
extern char _binary_demo_cnaf_bin_end[];

#ifdef CHASEROS_HAVE_SLIME_USER
extern char _binary_hello_sm_bin_start[];
extern char _binary_hello_sm_bin_end[];
#endif

void chaseros_user_jump_ring3(uint64_t rip, uint64_t rsp) {
    /*
     * Shell 在键盘 IRQ1 里调用用户程序；若直接 iretq 进 ring3，isr 末尾的 PIC EOI 永不到达，
     * 主片可能一直认为 IRQ1 未结束。QEMU 常能蒙混，VMware 上易导致后续 int $0x80 卡死。
     * IRQ1 属主 PIC，只发主片 EOI（0x20 -> 0x20）。
     */
    outb(0x20, 0x20);
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
    puts("\n[user] exited.\nChaserOS> ");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void user_run_cnaf_from_path_streaming(const char *abs_path) {
    (void)cndyn_run_package_from_path(abs_path);
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

    grant_user_regions();

    if (elf_load_flat(blob_start, sz, &entry) != 0) {
        puts("user: embedded ELF load failed ("); puts(tag); puts(")\n");
        return;
    }

    chaseros_active_user_pid = 2;
    process_bind_user_slot(chaseros_active_user_pid);
    user_fd_reset();
    chaseros_user_set_cwd(shell_get_cwd());
    puts("[kernel] jumping to user ("); puts(tag); puts(", ring 3)...\n");
    chaseros_user_jump_ring3(entry, USER_STACK_TOP);
}

void user_run_embedded_hello(void) {
    user_run_embedded_elf((const uint8_t *)_binary_hello_bin_start,
                         (const uint8_t *)_binary_hello_bin_end, "C hello");
}

void user_run_embedded_slime_hello(void) {
#ifdef CHASEROS_HAVE_SLIME_USER
    user_run_embedded_elf((const uint8_t *)_binary_hello_sm_bin_start,
                         (const uint8_t *)_binary_hello_sm_bin_end, "Slime hello");
#else
    puts("Slime user ELF not embedded. Reconfigure with -DCHASEROS_WITH_SLIME_USER=ON (needs slimec in PATH).\n");
#endif
}

void user_run_embedded_demo_cnaf(void) {
    const uint8_t *p = (const uint8_t *)_binary_demo_cnaf_bin_start;
    size_t n = (size_t)(_binary_demo_cnaf_bin_end - _binary_demo_cnaf_bin_start);
#ifdef CHASEROS_HAVE_SLIME_USER
    (void)cndyn_run_embedded_package(p, n, "embedded CNPKLOADER (Slime)");
#else
    (void)cndyn_run_embedded_package(p, n, "embedded demo.cnpk (C hello IMAGE)");
#endif
}

void user_run_cnaf_from_owned_pages(uint8_t *blob, size_t blob_sz, uint64_t blob_pages, const char *tag) {
    (void)blob_pages;
    (void)cndyn_run_embedded_package(blob, blob_sz, tag);
}

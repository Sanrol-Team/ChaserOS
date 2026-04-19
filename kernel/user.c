/* kernel/user.c - Ring 3 用户态跳转与嵌入的 hello 程序 */

#include "user.h"
#include "shell.h"
#include "user_fd.h"
#include "elf_load.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "fs/vfs.h"
#include "fs/cnaf/cnaf.h"
#include "fs/cnaf/cnaf_image.h"
#include "fs/cnaf/cnab.h"
#include "io.h"

#include <stddef.h>
#include <stdint.h>

#ifndef CNRUN_MAX_BYTES
#define CNRUN_MAX_BYTES (2u * 1024u * 1024u)
#endif
#define CNRUN_CNAF_HEADER_MAX 65536u

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

static void user_jump_to_ring3(uint64_t rip, uint64_t rsp) {
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

typedef struct {
    char path[256];
    uint32_t image_base;
} cnaf_image_stream_ctx_t;

static int cnaf_image_stream_read(void *ctx, uint64_t off_in_image, void *buf, size_t len) {
    cnaf_image_stream_ctx_t *c = ctx;
    if (len == 0) {
        return 0;
    }
    if (off_in_image > 0xFFFFFFFFull - (uint64_t)c->image_base) {
        return -1;
    }
    uint32_t foff = c->image_base + (uint32_t)off_in_image;
    uint8_t *bp = (uint8_t *)buf;
    size_t remain = len;
    while (remain > 0) {
        size_t chunk = remain > 4096u ? 4096u : remain;
        size_t got = 0;
        if (vfs_read_file_range(c->path, foff, bp, chunk, &got) != VFS_ERR_NONE || got != chunk) {
            return -1;
        }
        bp += chunk;
        foff += (uint32_t)chunk;
        remain -= chunk;
    }
    return 0;
}

void user_run_cnaf_from_path_streaming(const char *abs_path) {
    vfs_stat_t st;
    if (vfs_stat(abs_path, &st) != VFS_ERR_NONE) {
        puts("cnrun: not found or stat failed\n");
        return;
    }
    if (st.size == 0 || st.size > CNRUN_MAX_BYTES) {
        puts("cnrun: empty or too large\n");
        return;
    }

    uint8_t stack_prefix[4096];
    size_t first_sz = st.size < sizeof(stack_prefix) ? (size_t)st.size : sizeof(stack_prefix);
    size_t got = 0;
    if (vfs_read_file_range(abs_path, 0u, stack_prefix, first_sz, &got) != VFS_ERR_NONE ||
        got != first_sz) {
        puts("cnrun: read failed\n");
        return;
    }

    cnaf_file_header_t h;
    if (cnaf_parse_header_prefix(stack_prefix, first_sz, &h) != CNAF_OK) {
        puts("cnrun: not a valid CNAF header\n");
        return;
    }

    if (h.header_bytes > st.size || h.header_bytes > CNRUN_CNAF_HEADER_MAX) {
        puts("cnrun: invalid CNAF header_bytes\n");
        return;
    }

    uint8_t *hdr_buf = stack_prefix;
    uint8_t *hdr_heap = NULL;
    size_t hdr_len = (size_t)h.header_bytes;

    if (h.header_bytes > first_sz) {
        uint32_t pages =
            (uint32_t)(((size_t)h.header_bytes + PAGE_SIZE - 1u) / (size_t)PAGE_SIZE);
        hdr_heap = (uint8_t *)pmm_alloc_contiguous_pages(pages);
        if (!hdr_heap) {
            puts("cnrun: alloc header buffer failed\n");
            return;
        }
        if (vfs_read_file_range(abs_path, 0u, hdr_heap, (size_t)h.header_bytes, &got) != VFS_ERR_NONE ||
            got != (size_t)h.header_bytes) {
            pmm_free_contiguous(hdr_heap, pages);
            puts("cnrun: read CNAF header failed\n");
            return;
        }
        hdr_buf = hdr_heap;
    }

    uint64_t img_off = 0;
    uint64_t img_sz = 0;
    cnaf_err_t ce = cnaf_locate_image(hdr_buf, hdr_len, (size_t)st.size, &img_off, &img_sz);

    if (hdr_heap != NULL) {
        uint32_t pages =
            (uint32_t)(((size_t)h.header_bytes + PAGE_SIZE - 1u) / (size_t)PAGE_SIZE);
        pmm_free_contiguous(hdr_heap, pages);
    }

    if (ce != CNAF_OK) {
        puts("cnrun: CNAF IMAGE section not found\n");
        return;
    }
    if (img_off > 0xFFFFFFFFull || img_off + img_sz > st.size) {
        puts("cnrun: bad IMAGE layout\n");
        return;
    }

    cnaf_image_stream_ctx_t sctx;
    size_t pi = 0;
    while (abs_path[pi] && pi + 1 < sizeof(sctx.path)) {
        sctx.path[pi] = abs_path[pi];
        pi++;
    }
    sctx.path[pi] = '\0';
    sctx.image_base = (uint32_t)img_off;

    uint64_t entry = 0;
    grant_user_regions();
    if (cnab_load_streaming(cnaf_image_stream_read, &sctx, (size_t)img_sz, &entry) != 0) {
        puts("cnrun: CNAB load failed (IMAGE must be CNAB, not ELF)\n");
        return;
    }

    chaseros_active_user_pid = 2;
    process_bind_user_slot(chaseros_active_user_pid);
    user_fd_reset();
    chaseros_user_set_cwd(shell_get_cwd());
    puts("[kernel] jumping to user (CNAF stream, ring 3)...\n");
    user_jump_to_ring3(entry, USER_STACK_TOP);
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

    int lr = cnab_load_flat(img, img_sz, &entry);
    if (free_blob) {
        pmm_free_contiguous(free_blob, free_pages);
    }
    if (lr != 0) {
        puts("cnaf: CNAB load failed ("); puts(tag); puts(")\n");
        return;
    }

    chaseros_active_user_pid = 2;
    process_bind_user_slot(chaseros_active_user_pid);
    user_fd_reset();
    chaseros_user_set_cwd(shell_get_cwd());
    puts("[kernel] jumping to user ("); puts(tag); puts(", CNAF IMAGE=CNAB, ring 3)...\n");
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

    chaseros_active_user_pid = 2;
    process_bind_user_slot(chaseros_active_user_pid);
    user_fd_reset();
    chaseros_user_set_cwd(shell_get_cwd());
    puts("[kernel] jumping to user ("); puts(tag); puts(", ring 3)...\n");
    user_jump_to_ring3(entry, USER_STACK_TOP);
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
    user_run_cnaf_inner(p, n, "embedded CNAFLOADER (Slime)", NULL, 0);
#else
    user_run_cnaf_inner(p, n, "embedded demo.cnaf (C hello IMAGE)", NULL, 0);
#endif
}

void user_run_cnaf_from_owned_pages(uint8_t *blob, size_t blob_sz, uint64_t blob_pages, const char *tag) {
    user_run_cnaf_inner(blob, blob_sz, tag, blob, blob_pages);
}

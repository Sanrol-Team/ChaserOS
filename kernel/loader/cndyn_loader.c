#include "loader/cndyn_loader.h"

#include "console.h"
#include "fs/cnaf/cnaf.h"
#include "fs/cnaf/cnaf_image.h"
#include "fs/cnaf/cnab.h"
#include "fs/vfs.h"
#include "pmm.h"
#include "process.h"
#include "shell.h"
#include "user.h"
#include "user_fd.h"
#include "vmm.h"

#define CNDYN_MAX_FILE_BYTES (2u * 1024u * 1024u)
#define CNDYN_MAX_HEADER_BYTES 65536u
#define CNDYN_MAX_LOADED_LIBS 16u

typedef struct {
    char soname[64];
    uint32_t abi_major;
    uint32_t abi_minor;
    uint64_t base;
    int used;
} cndyn_lib_slot_t;

typedef struct {
    char path[256];
    uint32_t image_base;
} cndyn_stream_ctx_t;

static cndyn_lib_slot_t g_libs[CNDYN_MAX_LOADED_LIBS];
extern void puts(const char *s);

static int cndyn_stream_read(void *ctx, uint64_t off, void *buf, size_t len) {
    cndyn_stream_ctx_t *c = (cndyn_stream_ctx_t *)ctx;
    size_t got = 0;
    if (vfs_read_file_range(c->path, c->image_base + (uint32_t)off, buf, len, &got) != VFS_ERR_NONE || got != len) {
        return -1;
    }
    return 0;
}

static int str_eq(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void str_copy(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (cap == 0) return;
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int user_copy_cstr(uint64_t user_ptr, char *out, size_t cap) {
    size_t i;
    if (cap == 0) return -1;
    for (i = 0; i + 1 < cap; i++) {
        char c = ((const char *)(uintptr_t)user_ptr)[i];
        out[i] = c;
        if (c == '\0') return 0;
    }
    out[cap - 1] = '\0';
    return -1;
}

static int load_prefix(const char *abs_path, uint8_t *stack_prefix, size_t *out_file_size, cnpkg_file_header_t *out_h) {
    vfs_stat_t st;
    if (vfs_stat(abs_path, &st) != VFS_ERR_NONE) {
        return -1;
    }
    if (st.size == 0 || st.size > CNDYN_MAX_FILE_BYTES) {
        return -1;
    }
    size_t first_sz = st.size < 4096u ? (size_t)st.size : 4096u;
    size_t got = 0;
    if (vfs_read_file_range(abs_path, 0u, stack_prefix, first_sz, &got) != VFS_ERR_NONE || got != first_sz) {
        return -1;
    }
    if (cnpkg_parse_header_prefix(stack_prefix, first_sz, out_h) != CNPKG_OK) {
        return -1;
    }
    *out_file_size = (size_t)st.size;
    return 0;
}

static int read_header_full(const char *abs_path, const uint8_t *prefix, size_t prefix_len,
                            const cnpkg_file_header_t *h, uint8_t **out_hdr_buf, uint32_t *out_pages) {
    size_t got = 0;
    if (h->header_bytes > CNDYN_MAX_HEADER_BYTES || h->header_bytes < sizeof(*h)) {
        return -1;
    }
    if (h->header_bytes <= prefix_len) {
        *out_hdr_buf = (uint8_t *)prefix;
        *out_pages = 0;
        return 0;
    }
    uint32_t pages = (uint32_t)((h->header_bytes + PAGE_SIZE - 1u) / PAGE_SIZE);
    uint8_t *buf = (uint8_t *)pmm_alloc_contiguous_pages(pages);
    if (!buf) return -1;
    if (vfs_read_file_range(abs_path, 0u, buf, h->header_bytes, &got) != VFS_ERR_NONE || got != h->header_bytes) {
        pmm_free_contiguous(buf, pages);
        return -1;
    }
    *out_hdr_buf = buf;
    *out_pages = pages;
    return 0;
}

static int parse_u32(const char *s, uint32_t *out) {
    uint32_t v = 0;
    size_t i = 0;
    if (!s || !s[0]) return -1;
    while (s[i]) {
        char c = s[i];
        if (c < '0' || c > '9') return -1;
        v = v * 10u + (uint32_t)(c - '0');
        i++;
    }
    *out = v;
    return 0;
}

static int resolve_requires(const uint8_t *manifest, size_t manifest_len) {
    char reqs[192];
    if (cnpkg_manifest_get_value(manifest, manifest_len, CNPKG_MANIFEST_KEY_REQUIRES, reqs, sizeof(reqs)) != 0) {
        return 0;
    }
    size_t i = 0;
    while (reqs[i]) {
        char lib[48];
        char majs[16];
        size_t li = 0, mi = 0;
        while (reqs[i] == ' ' || reqs[i] == ',') i++;
        while (reqs[i] && reqs[i] != '@' && reqs[i] != ',') {
            if (li + 1 < sizeof(lib)) lib[li++] = reqs[i];
            i++;
        }
        lib[li] = '\0';
        if (reqs[i] != '@') return -1;
        i++;
        while (reqs[i] && reqs[i] != '.' && reqs[i] != ',') {
            if (mi + 1 < sizeof(majs)) majs[mi++] = reqs[i];
            i++;
        }
        majs[mi] = '\0';
        while (reqs[i] && reqs[i] != ',') i++;
        if (lib[0] == '\0' || majs[0] == '\0') return -1;
        uint32_t abi_major = 0;
        if (parse_u32(majs, &abi_major) != 0) return -1;
        char path[160] = CNPKG_SYS_LIB_DIR "/";
        size_t p = str_len(path);
        for (size_t k = 0; lib[k] && p + 6 < sizeof(path); k++) path[p++] = lib[k];
        path[p++] = '.';
        path[p++] = 'c';
        path[p++] = 'n';
        path[p++] = 'l';
        path[p++] = 'k';
        path[p] = '\0';
        vfs_stat_t st;
        if (vfs_stat(path, &st) != VFS_ERR_NONE) {
            return -1;
        }
        (void)abi_major;
    }
    return 0;
}

int cndyn_load_package_for_exec(const char *abs_path, uint64_t *out_entry) {
    uint8_t prefix[4096];
    size_t file_size = 0;
    cnpkg_file_header_t h;
    if (load_prefix(abs_path, prefix, &file_size, &h) != 0 || h.pkg_kind != CNPKG_KIND_PROGRAM) {
        puts("cnrun: invalid .cnpk\n");
        return -1;
    }
    uint8_t *hdr = NULL;
    uint32_t pages = 0;
    if (read_header_full(abs_path, prefix, sizeof(prefix), &h, &hdr, &pages) != 0) {
        puts("cnrun: header read failed\n");
        return -1;
    }
    uint64_t image_off = 0, image_sz = 0, man_off = 0, man_sz = 0;
    if (cnpkg_locate_section(hdr, h.header_bytes, file_size, CNPKG_SECTION_IMAGE, &image_off, &image_sz) != CNPKG_OK ||
        cnpkg_locate_section(hdr, h.header_bytes, file_size, CNPKG_SECTION_MANIFEST, &man_off, &man_sz) != CNPKG_OK) {
        if (pages) pmm_free_contiguous(hdr, pages);
        puts("cnrun: missing section\n");
        return -1;
    }
    if (man_sz > 4096u) {
        if (pages) pmm_free_contiguous(hdr, pages);
        puts("cnrun: manifest too large\n");
        return -1;
    }
    uint8_t manifest[4096];
    size_t got = 0;
    if (vfs_read_file_range(abs_path, (uint32_t)man_off, manifest, (size_t)man_sz, &got) != VFS_ERR_NONE || got != man_sz) {
        if (pages) pmm_free_contiguous(hdr, pages);
        puts("cnrun: manifest read failed\n");
        return -1;
    }
    if (resolve_requires(manifest, (size_t)man_sz) != 0) {
        if (pages) pmm_free_contiguous(hdr, pages);
        puts("cnrun: dependency/abi check failed\n");
        return -1;
    }
    if (pages) pmm_free_contiguous(hdr, pages);
    cndyn_stream_ctx_t sctx;
    str_copy(sctx.path, sizeof(sctx.path), abs_path);
    if (image_off > 0xFFFFFFFFull || image_sz > CNDYN_MAX_FILE_BYTES) {
        puts("cnrun: bad image layout\n");
        return -1;
    }
    sctx.image_base = (uint32_t)image_off;
    uint64_t entry = 0;
    vmm_grant_user_2mb_region(0x400000ULL, 0);
    vmm_grant_user_2mb_region(0x801008ULL - 4096ULL, 1);
    vmm_flush_tlb_all();
    if (cnim_load_streaming(cndyn_stream_read, &sctx, (size_t)image_sz, &entry) != 0) {
        puts("cnrun: CNIM load failed\n");
        return -1;
    }
    if (out_entry) {
        *out_entry = entry;
    }
    return 0;
}

int cndyn_run_package_from_path(const char *abs_path) {
    uint64_t entry = 0;
    if (cndyn_load_package_for_exec(abs_path, &entry) != 0) {
        return -1;
    }
    chaseros_active_user_pid = 2;
    process_bind_user_slot(chaseros_active_user_pid);
    user_fd_reset();
    chaseros_user_set_cwd(shell_get_cwd());
    puts("[kernel] jumping to user (CNPK/CNIM, ring 3)...\n");
    /* reuse existing entry via user.c */
    extern void chaseros_user_jump_ring3(uint64_t rip, uint64_t rsp);
    chaseros_user_jump_ring3(entry, 0x801008ULL);
    return 0;
}

int cndyn_run_embedded_package(const uint8_t *blob, size_t blob_sz, const char *tag) {
    const uint8_t *img = NULL, *manifest = NULL;
    size_t img_sz = 0, man_sz = 0;
    if (cnpkg_extract_section(blob, blob_sz, CNPKG_SECTION_IMAGE, &img, &img_sz) != CNPKG_OK ||
        cnpkg_extract_section(blob, blob_sz, CNPKG_SECTION_MANIFEST, &manifest, &man_sz) != CNPKG_OK) {
        puts("cnrun: embedded package invalid\n");
        return -1;
    }
    if (resolve_requires(manifest, man_sz) != 0) {
        puts("cnrun: embedded dependency check failed\n");
        return -1;
    }
    uint64_t entry = 0;
    vmm_grant_user_2mb_region(0x400000ULL, 0);
    vmm_grant_user_2mb_region(0x801008ULL - 4096ULL, 1);
    vmm_flush_tlb_all();
    if (cnim_load_flat(img, img_sz, &entry) != 0) {
        puts("cnrun: embedded CNIM load failed\n");
        return -1;
    }
    chaseros_active_user_pid = 2;
    process_bind_user_slot(chaseros_active_user_pid);
    user_fd_reset();
    chaseros_user_set_cwd(shell_get_cwd());
    puts("[kernel] jumping to user (");
    puts(tag ? tag : "embedded CNPK");
    puts(", ring 3)...\n");
    extern void chaseros_user_jump_ring3(uint64_t rip, uint64_t rsp);
    chaseros_user_jump_ring3(entry, 0x801008ULL);
    return 0;
}

int cndyn_dlopen(uint64_t user_path_ptr, uint64_t mode) {
    char path[192];
    (void)mode;
    if (user_copy_cstr(user_path_ptr, path, sizeof(path)) != 0) {
        return -1;
    }
    for (uint32_t i = 0; i < CNDYN_MAX_LOADED_LIBS; i++) {
        if (g_libs[i].used && str_eq(g_libs[i].soname, path)) {
            return (int)(i + 1u);
        }
    }
    for (uint32_t i = 0; i < CNDYN_MAX_LOADED_LIBS; i++) {
        if (!g_libs[i].used) {
            g_libs[i].used = 1;
            str_copy(g_libs[i].soname, sizeof(g_libs[i].soname), path);
            g_libs[i].abi_major = 1;
            g_libs[i].abi_minor = 0;
            g_libs[i].base = 0;
            return (int)(i + 1u);
        }
    }
    return -1;
}

int cndyn_dlsym(uint64_t handle, uint64_t user_symbol_ptr) {
    (void)user_symbol_ptr;
    if (handle == 0 || handle > CNDYN_MAX_LOADED_LIBS) {
        return -1;
    }
    if (!g_libs[handle - 1u].used) {
        return -1;
    }
    return 0;
}

int cndyn_dlclose(uint64_t handle) {
    if (handle == 0 || handle > CNDYN_MAX_LOADED_LIBS) {
        return -1;
    }
    g_libs[handle - 1u].used = 0;
    return 0;
}

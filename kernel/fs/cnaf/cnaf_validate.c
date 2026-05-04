/* CNPK/CNLK 文件头和清单解析（无 libc） */

#include "fs/cnaf/cnaf.h"

#include <stddef.h>

static void chaseros_memcpy(void *d, const void *s, size_t n) {
    unsigned char *a = d;
    const unsigned char *b = s;
    while (n--) {
        *a++ = *b++;
    }
}

cnpkg_err_t cnpkg_probe_header(const void *data, size_t len, cnpkg_file_header_t *out) {
    cnpkg_file_header_t h;

    if (!data) {
        return CNPKG_ERR_TRUNCATED;
    }
    if (len < sizeof(h)) {
        return CNPKG_ERR_TRUNCATED;
    }
    chaseros_memcpy(&h, data, sizeof(h));

    if (h.magic != CNPKG_MAGIC_CNPK && h.magic != CNPKG_MAGIC_CNLK) {
        return CNPKG_ERR_MAGIC;
    }
    if (h.fmt_major != CNPKG_FMT_MAJOR) {
        return CNPKG_ERR_VERSION;
    }
    if (h.fmt_minor > CNPKG_FMT_MINOR) {
        return CNPKG_ERR_VERSION;
    }
    if (h.pkg_kind != CNPKG_KIND_PROGRAM && h.pkg_kind != CNPKG_KIND_LIBRARY) {
        return CNPKG_ERR_KIND;
    }
    if (h.header_bytes < sizeof(h)) {
        return CNPKG_ERR_TRUNCATED;
    }
    if (h.header_bytes > len) {
        return CNPKG_ERR_TRUNCATED;
    }
    if ((h.header_bytes & (CNPKG_HEADER_ALIGN - 1u)) != 0u) {
        return CNPKG_ERR_SECTION;
    }
    if (h.section_count > 256u) {
        return CNPKG_ERR_SECTION;
    }

    if (out) {
        *out = h;
    }
    return CNPKG_OK;
}

cnpkg_err_t cnpkg_parse_header_prefix(const void *data, size_t len, cnpkg_file_header_t *out) {
    cnpkg_file_header_t h;

    if (!data || !out) {
        return CNPKG_ERR_TRUNCATED;
    }
    if (len < sizeof(h)) {
        return CNPKG_ERR_TRUNCATED;
    }
    chaseros_memcpy(&h, data, sizeof(h));

    if (h.magic != CNPKG_MAGIC_CNPK && h.magic != CNPKG_MAGIC_CNLK) {
        return CNPKG_ERR_MAGIC;
    }
    if (h.fmt_major != CNPKG_FMT_MAJOR) {
        return CNPKG_ERR_VERSION;
    }
    if (h.fmt_minor > CNPKG_FMT_MINOR) {
        return CNPKG_ERR_VERSION;
    }
    if (h.pkg_kind != CNPKG_KIND_PROGRAM && h.pkg_kind != CNPKG_KIND_LIBRARY) {
        return CNPKG_ERR_KIND;
    }
    if (h.header_bytes < sizeof(h)) {
        return CNPKG_ERR_TRUNCATED;
    }

    *out = h;
    return CNPKG_OK;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static int key_match(const char *a, size_t n, const char *b) {
    size_t i = 0;
    while (b[i] != '\0') {
        if (i >= n || a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return i == n;
}

int cnpkg_manifest_get_value(const uint8_t *manifest, size_t manifest_len, const char *key,
                             char *out, size_t out_cap) {
    size_t i = 0;
    if (!manifest || !key || !out || out_cap == 0) {
        return -1;
    }
    while (i < manifest_len) {
        size_t line_beg = i;
        size_t line_end = i;
        while (line_end < manifest_len && manifest[line_end] != '\n') {
            line_end++;
        }
        i = (line_end < manifest_len) ? (line_end + 1u) : line_end;
        while (line_beg < line_end && is_ws((char)manifest[line_beg])) {
            line_beg++;
        }
        if (line_beg >= line_end || manifest[line_beg] == '#') {
            continue;
        }
        size_t eq = line_beg;
        while (eq < line_end && manifest[eq] != '=') {
            eq++;
        }
        if (eq >= line_end) {
            continue;
        }
        size_t key_end = eq;
        while (key_end > line_beg && is_ws((char)manifest[key_end - 1u])) {
            key_end--;
        }
        if (!key_match((const char *)manifest + line_beg, key_end - line_beg, key)) {
            continue;
        }
        size_t val_beg = eq + 1u;
        while (val_beg < line_end && is_ws((char)manifest[val_beg])) {
            val_beg++;
        }
        size_t val_end = line_end;
        while (val_end > val_beg && is_ws((char)manifest[val_end - 1u])) {
            val_end--;
        }
        size_t n = val_end - val_beg;
        if (n + 1u > out_cap) {
            return -1;
        }
        for (size_t k = 0; k < n; k++) {
            out[k] = (char)manifest[val_beg + k];
        }
        out[n] = '\0';
        return 0;
    }
    return -1;
}

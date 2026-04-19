/* CNAF/CNAFL 文件头校验（无 libc） */

#include "fs/cnaf/cnaf.h"

#include <stddef.h>

static void chaseros_memcpy(void *d, const void *s, size_t n) {
    unsigned char *a = d;
    const unsigned char *b = s;
    while (n--) {
        *a++ = *b++;
    }
}

cnaf_err_t cnaf_probe_header(const void *data, size_t len, cnaf_file_header_t *out) {
    cnaf_file_header_t h;

    if (!data) {
        return CNAF_ERR_TRUNCATED;
    }
    if (len < sizeof(h)) {
        return CNAF_ERR_TRUNCATED;
    }
    chaseros_memcpy(&h, data, sizeof(h));

    if (h.magic != CNAF_MAGIC_U32) {
        return CNAF_ERR_MAGIC;
    }
    if (h.fmt_major != CNAF_FMT_MAJOR) {
        return CNAF_ERR_VERSION;
    }
    if (h.fmt_minor > CNAF_FMT_MINOR) {
        return CNAF_ERR_VERSION;
    }
    if (h.payload_kind != CNAF_PAYLOAD_APP && h.payload_kind != CNAF_PAYLOAD_LIB) {
        return CNAF_ERR_KIND;
    }
    if (h.header_bytes < sizeof(h)) {
        return CNAF_ERR_TRUNCATED;
    }
    if (h.header_bytes > len) {
        return CNAF_ERR_TRUNCATED;
    }

    if (out) {
        *out = h;
    }
    return CNAF_OK;
}

cnaf_err_t cnaf_parse_header_prefix(const void *data, size_t len, cnaf_file_header_t *out) {
    cnaf_file_header_t h;

    if (!data || !out) {
        return CNAF_ERR_TRUNCATED;
    }
    if (len < sizeof(h)) {
        return CNAF_ERR_TRUNCATED;
    }
    chaseros_memcpy(&h, data, sizeof(h));

    if (h.magic != CNAF_MAGIC_U32) {
        return CNAF_ERR_MAGIC;
    }
    if (h.fmt_major != CNAF_FMT_MAJOR) {
        return CNAF_ERR_VERSION;
    }
    if (h.fmt_minor > CNAF_FMT_MINOR) {
        return CNAF_ERR_VERSION;
    }
    if (h.payload_kind != CNAF_PAYLOAD_APP && h.payload_kind != CNAF_PAYLOAD_LIB) {
        return CNAF_ERR_KIND;
    }
    if (h.header_bytes < sizeof(h)) {
        return CNAF_ERR_TRUNCATED;
    }

    *out = h;
    return CNAF_OK;
}

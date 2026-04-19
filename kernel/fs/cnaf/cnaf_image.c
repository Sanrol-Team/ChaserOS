/* CNAF/CNAFL — IMAGE 节定位（v0.2 节表或 v0.1 遗留） */

#include "fs/cnaf/cnaf_image.h"
#include "fs/cnaf/cnaf.h"

#include <stddef.h>
#include <stdint.h>

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd_u64_le(const uint8_t *p) {
    uint64_t lo = rd_u32_le(p);
    uint64_t hi = rd_u32_le(p + 4);
    return lo | (hi << 32);
}

cnaf_err_t cnaf_locate_image(const uint8_t *prefix, size_t prefix_len, size_t file_total_size,
                             uint64_t *out_image_offset, uint64_t *out_image_size) {
    cnaf_file_header_t h;
    cnaf_err_t pr = cnaf_probe_header(prefix, prefix_len, &h);
    if (pr != CNAF_OK) {
        return pr;
    }
    if (h.header_bytes > prefix_len) {
        return CNAF_ERR_TRUNCATED;
    }
    if (h.header_bytes > file_total_size) {
        return CNAF_ERR_TRUNCATED;
    }

    if (h.flags & CNAF_FLAG_SECTION_TABLE) {
        uint32_t count = rd_u32_le(prefix + 36);
        size_t entry_base = 44;
        for (uint32_t i = 0; i < count; i++) {
            size_t eoff = entry_base + (size_t)i * 24u;
            if (eoff + 24 > prefix_len) {
                return CNAF_ERR_SECTION;
            }
            uint32_t type = rd_u32_le(prefix + eoff);
            if (type != (uint32_t)CNAF_SECTION_IMAGE) {
                continue;
            }
            uint64_t off = rd_u64_le(prefix + eoff + 8);
            uint64_t sz = rd_u64_le(prefix + eoff + 16);
            if (sz > (uint64_t)SIZE_MAX) {
                return CNAF_ERR_SECTION;
            }
            if (off + sz > file_total_size || off + sz < off) {
                return CNAF_ERR_SECTION;
            }
            *out_image_offset = off;
            *out_image_size = sz;
            return CNAF_OK;
        }
        return CNAF_ERR_SECTION;
    }

    if (h.header_bytes > file_total_size) {
        return CNAF_ERR_TRUNCATED;
    }
    *out_image_offset = h.header_bytes;
    *out_image_size = file_total_size - h.header_bytes;
    return CNAF_OK;
}

cnaf_err_t cnaf_extract_image(const uint8_t *data, size_t len, const uint8_t **out_image,
                              size_t *out_image_size) {
    uint64_t off = 0;
    uint64_t sz = 0;
    cnaf_err_t e = cnaf_locate_image(data, len, len, &off, &sz);
    if (e != CNAF_OK) {
        return e;
    }
    if (off + sz > len || off + sz < off) {
        return CNAF_ERR_SECTION;
    }
    *out_image = data + (size_t)off;
    *out_image_size = (size_t)sz;
    return CNAF_OK;
}

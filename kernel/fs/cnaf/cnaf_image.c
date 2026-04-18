/* CNAF/CNAFL — IMAGE 节提取 */

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

cnaf_err_t cnaf_extract_image(const uint8_t *data, size_t len, const uint8_t **out_image,
                              size_t *out_image_size) {
    cnaf_file_header_t h;
    cnaf_err_t pr = cnaf_probe_header(data, len, &h);
    if (pr != CNAF_OK) {
        return pr;
    }

    if (h.flags & CNAF_FLAG_SECTION_TABLE) {
        if (len < h.header_bytes) {
            return CNAF_ERR_TRUNCATED;
        }
        if (len < 44) {
            return CNAF_ERR_TRUNCATED;
        }
        uint32_t count = rd_u32_le(data + 36);
        size_t entry_base = 44;
        for (uint32_t i = 0; i < count; i++) {
            size_t eoff = entry_base + (size_t)i * 24u;
            if (eoff + 24 > len) {
                return CNAF_ERR_SECTION;
            }
            uint32_t type = rd_u32_le(data + eoff);
            if (type != (uint32_t)CNAF_SECTION_IMAGE) {
                continue;
            }
            uint64_t off = rd_u64_le(data + eoff + 8);
            uint64_t sz = rd_u64_le(data + eoff + 16);
            if (sz > (uint64_t)SIZE_MAX) {
                return CNAF_ERR_SECTION;
            }
            if (off + sz > len || off + sz < off) {
                return CNAF_ERR_SECTION;
            }
            *out_image = data + (size_t)off;
            *out_image_size = (size_t)sz;
            return CNAF_OK;
        }
        return CNAF_ERR_SECTION;
    }

    /* 遗留：header 之后直至 EOF 为整段 IMAGE */
    if (h.header_bytes > len) {
        return CNAF_ERR_TRUNCATED;
    }
    *out_image = data + h.header_bytes;
    *out_image_size = len - h.header_bytes;
    return CNAF_OK;
}

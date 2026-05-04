/* CNPK/CNLK section定位。 */

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

cnpkg_err_t cnpkg_locate_section(const uint8_t *prefix, size_t prefix_len, size_t file_total_size,
                                 uint32_t type, uint64_t *out_offset, uint64_t *out_size) {
    cnpkg_file_header_t h;
    cnpkg_err_t pr = cnpkg_probe_header(prefix, prefix_len, &h);
    if (pr != CNPKG_OK) {
        return pr;
    }
    if (h.header_bytes > prefix_len) {
        return CNPKG_ERR_TRUNCATED;
    }
    if (h.header_bytes > file_total_size) {
        return CNPKG_ERR_TRUNCATED;
    }
    size_t entry_base = sizeof(cnpkg_file_header_t);
    for (uint32_t i = 0; i < h.section_count; i++) {
        size_t eoff = entry_base + (size_t)i * sizeof(cnpkg_section_entry_t);
        if (eoff + sizeof(cnpkg_section_entry_t) > prefix_len) {
            return CNPKG_ERR_SECTION;
        }
        uint32_t cur_type = rd_u32_le(prefix + eoff);
        if (cur_type != type) {
            continue;
        }
        uint64_t off = rd_u64_le(prefix + eoff + 8);
        uint64_t sz = rd_u64_le(prefix + eoff + 16);
        if (sz > (uint64_t)SIZE_MAX) {
            return CNPKG_ERR_SECTION;
        }
        if (off + sz > file_total_size || off + sz < off) {
            return CNPKG_ERR_SECTION;
        }
        *out_offset = off;
        *out_size = sz;
        return CNPKG_OK;
    }
    return CNPKG_ERR_SECTION;
}

cnpkg_err_t cnpkg_extract_section(const uint8_t *data, size_t len, uint32_t type,
                                  const uint8_t **out_ptr, size_t *out_size) {
    uint64_t off = 0;
    uint64_t sz = 0;
    cnpkg_err_t e = cnpkg_locate_section(data, len, len, type, &off, &sz);
    if (e != CNPKG_OK) {
        return e;
    }
    if (off + sz > len || off + sz < off) {
        return CNPKG_ERR_SECTION;
    }
    *out_ptr = data + (size_t)off;
    *out_size = (size_t)sz;
    return CNPKG_OK;
}

cnpkg_err_t cnpkg_locate_image(const uint8_t *prefix, size_t prefix_len, size_t file_total_size,
                               uint64_t *out_image_offset, uint64_t *out_image_size) {
    return cnpkg_locate_section(prefix, prefix_len, file_total_size, (uint32_t)CNPKG_SECTION_IMAGE,
                                out_image_offset, out_image_size);
}

cnpkg_err_t cnpkg_extract_image(const uint8_t *data, size_t len, const uint8_t **out_image,
                                size_t *out_image_size) {
    return cnpkg_extract_section(data, len, (uint32_t)CNPKG_SECTION_IMAGE, out_image, out_image_size);
}

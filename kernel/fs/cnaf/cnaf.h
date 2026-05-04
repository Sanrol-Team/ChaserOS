/* ChaserOS DLL & Application General Format parser API. */

#ifndef CHASEROS_CNAF_H
#define CHASEROS_CNAF_H

#include <stddef.h>

#include "cnaf_spec.h"

/*
 * 校验缓冲区开头是否为合法 CNPK/CNLK 文件头。
 * data 可任意对齐；len 为可用字节数。
 * out 可为 NULL；若返回 CNAF_OK 且 out 非空，则写入解析后的头（小端按本地 CPU 已还原）。
 */
cnpkg_err_t cnpkg_probe_header(const void *data, size_t len, cnpkg_file_header_t *out);

/**
 * 仅从头前缀解析文件头（不要求 len >= header_bytes）。
 * 用于流式装载：先读小块得到 header_bytes，再读满节表。
 */
cnpkg_err_t cnpkg_parse_header_prefix(const void *data, size_t len, cnpkg_file_header_t *out);

cnpkg_err_t cnpkg_locate_section(const uint8_t *prefix, size_t prefix_len, size_t file_total_size,
                                 uint32_t type, uint64_t *out_offset, uint64_t *out_size);

cnpkg_err_t cnpkg_extract_section(const uint8_t *data, size_t len, uint32_t type,
                                  const uint8_t **out_ptr, size_t *out_size);

int cnpkg_manifest_get_value(const uint8_t *manifest, size_t manifest_len, const char *key,
                             char *out, size_t out_cap);

#endif

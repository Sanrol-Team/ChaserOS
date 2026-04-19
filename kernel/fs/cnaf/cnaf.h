/* CNAF/CNAFL 内核侧 API（Phase A：头解析） */

#ifndef CHASEROS_CNAF_H
#define CHASEROS_CNAF_H

#include <stddef.h>

#include "cnaf_spec.h"

/*
 * 校验缓冲区开头是否为合法 CNAF/CNAFL 文件头。
 * data 可任意对齐；len 为可用字节数。
 * out 可为 NULL；若返回 CNAF_OK 且 out 非空，则写入解析后的头（小端按本地 CPU 已还原）。
 */
cnaf_err_t cnaf_probe_header(const void *data, size_t len, cnaf_file_header_t *out);

/**
 * 仅从至少 36 字节前缀解析文件头（不要求 len >= header_bytes）。
 * 用于流式装载：先读小块得到 header_bytes，再读满节表。
 */
cnaf_err_t cnaf_parse_header_prefix(const void *data, size_t len, cnaf_file_header_t *out);

#endif

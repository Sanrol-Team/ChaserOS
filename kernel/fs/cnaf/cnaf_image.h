/* CNAF/CNAFL — 从缓冲区提取 IMAGE 节（v0.2 节表或 v0.1 遗留布局） */

#ifndef CHASEROS_CNAF_IMAGE_H
#define CHASEROS_CNAF_IMAGE_H

#include "cnaf_spec.h"
#include <stddef.h>
#include <stdint.h>

/*
 * 仅从「文件开头起 prefix_len 字节」与「文件总大小」解析 IMAGE 在整文件中的偏移与长度。
 * 调用方须保证 prefix_len >= cnaf_file_header_t::header_bytes（可先读头再按需补读）。
 */
cnaf_err_t cnaf_locate_image(const uint8_t *prefix, size_t prefix_len, size_t file_total_size,
                             uint64_t *out_image_offset, uint64_t *out_image_size);

/*
 * 在已通过 cnaf_probe_header 的连续缓冲中定位 IMAGE（整包在内存）。
 * 成功时 *out_image 指向 data 内部（调用方勿释放），*out_image_size 为字节数。
 */
cnaf_err_t cnaf_extract_image(const uint8_t *data, size_t len, const uint8_t **out_image,
                              size_t *out_image_size);

#endif

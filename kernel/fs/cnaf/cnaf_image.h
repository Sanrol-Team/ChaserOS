/* CNAF/CNAFL — 从缓冲区提取 IMAGE 节（v0.2 节表或 v0.1 遗留布局） */

#ifndef CNOS_CNAF_IMAGE_H
#define CNOS_CNAF_IMAGE_H

#include "cnaf_spec.h"
#include <stddef.h>
#include <stdint.h>

/*
 * 在已通过 cnaf_probe_header 的负载中定位 IMAGE 节。
 * 成功时 *out_image 指向 data 内部（调用方勿释放），*out_image_size 为字节数。
 */
cnaf_err_t cnaf_extract_image(const uint8_t *data, size_t len, const uint8_t **out_image,
                              size_t *out_image_size);

#endif

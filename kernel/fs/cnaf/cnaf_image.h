/* CNPK/CNLK section locator helpers. */

#ifndef CHASEROS_CNAF_IMAGE_H
#define CHASEROS_CNAF_IMAGE_H

#include "cnaf_spec.h"
#include <stddef.h>
#include <stdint.h>

cnpkg_err_t cnpkg_locate_image(const uint8_t *prefix, size_t prefix_len, size_t file_total_size,
                               uint64_t *out_image_offset, uint64_t *out_image_size);

cnpkg_err_t cnpkg_extract_image(const uint8_t *data, size_t len, const uint8_t **out_image,
                                size_t *out_image_size);

#endif

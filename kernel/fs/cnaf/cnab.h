/* CNIM — CNPK/CNLK IMAGE section payload. */

#ifndef CHASEROS_CNAB_H
#define CHASEROS_CNAB_H

#include <stddef.h>
#include <stdint.h>

#include "cnaf_spec.h"

typedef int (*cnim_stream_read_t)(void *ctx, uint64_t off_in_image, void *buf, size_t len);

int cnim_load_flat(const void *image, size_t image_size, uint64_t *out_entry);
int cnim_load_streaming(cnim_stream_read_t read_fn, void *ctx, size_t image_size, uint64_t *out_entry);

/* compatibility aliases for old call sites */
typedef cnim_stream_read_t cnab_stream_read_t;
#define cnab_load_flat cnim_load_flat
#define cnab_load_streaming cnim_load_streaming

#endif

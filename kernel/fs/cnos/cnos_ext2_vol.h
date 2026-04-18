/* 最小 ext2 卷：单块组、1024 块大小、与 CNOS RAM/IDE 后端对接 */

#ifndef CNOS_EXT2_VOL_H
#define CNOS_EXT2_VOL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    CNOS_VOL_NONE = 0,
    CNOS_VOL_RAM,
    CNOS_VOL_IDE
} cnos_vol_type_t;

typedef struct cnos_vol {
    cnos_vol_type_t type;
    uint8_t *ram_base;
    size_t size_bytes;
    uint8_t ide_drive;
} cnos_vol_t;

void cnos_vol_init(void);
const cnos_vol_t *cnos_vol_current(void);

/*
 * 附加 RAM 卷（连续物理页）。长度须为 1024 的倍数，且 >= 256KiB，且块数 <= 8192。
 */
int cnos_vol_attach_ram(void *base, size_t size_bytes);
void cnos_vol_detach_ram(void);

/* 释放当前卷（RAM 或 IDE 句柄），便于切换后端 */
void cnos_vol_detach_all(void);

/* IDE：drive 0=主 1=从，容量来自 IDENTIFY */
int cnos_vol_attach_ide(uint8_t drive);

int cnos_ext2_format(const cnos_vol_t *v);
int cnos_ext2_ls(const cnos_vol_t *v);
int cnos_ext2_read_file(const cnos_vol_t *v, const char *name, char *buf, size_t buf_sz,
                        size_t *out_len);
int cnos_ext2_write_file(const cnos_vol_t *v, const char *name, const char *data, size_t len);

#endif

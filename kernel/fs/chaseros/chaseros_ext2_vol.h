/* 最小 ext2 卷：单块组、1024 块大小、与 ChaserOS RAM/IDE 后端对接 */

#ifndef CHASEROS_EXT2_VOL_H
#define CHASEROS_EXT2_VOL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    CHASEROS_VOL_NONE = 0,
    CHASEROS_VOL_RAM,
    CHASEROS_VOL_IDE,
    CHASEROS_VOL_AHCI
} chaseros_vol_type_t;

typedef struct chaseros_vol {
    chaseros_vol_type_t type;
    uint8_t *ram_base;
    size_t size_bytes;
    uint8_t ide_drive;
    uint32_t ahci_port;
} chaseros_vol_t;

void chaseros_vol_init(void);
const chaseros_vol_t *chaseros_vol_current(void);

/** 规范化绝对路径（须以 '/' 开头），折叠 . 与 .. */
int chaseros_path_normalize(const char *in, char *out, size_t outsz);

/*
 * 附加 RAM 卷（连续物理页）。长度须为 1024 的倍数，且 >= 256KiB，且块数 <= 8192。
 */
int chaseros_vol_attach_ram(void *base, size_t size_bytes);
void chaseros_vol_detach_ram(void);

/* 释放当前卷（RAM 或 IDE 句柄），便于切换后端 */
void chaseros_vol_detach_all(void);

/* IDE：drive 0=主 1=从，容量来自 IDENTIFY */
int chaseros_vol_attach_ide(uint8_t drive);

#ifdef CHASEROS_HAVE_AHCI_RUST
/* AHCI：port 0..31，须已执行 chaseros_ahci_pci_probe 且该口有盘 */
int chaseros_vol_attach_ahci(uint32_t port);
#endif

int chaseros_ext2_format(const chaseros_vol_t *v);
/** 列出目录 path（绝对路径，已规范化，如 "/" 或 "/foo"） */
int chaseros_ext2_ls_at(const chaseros_vol_t *v, const char *path);
int chaseros_ext2_ls(const chaseros_vol_t *v);
int chaseros_ext2_stat_file(const chaseros_vol_t *v, const char *name, uint32_t *out_size);
int chaseros_ext2_read_file_range(const chaseros_vol_t *v, const char *name, uint32_t offset, void *buf,
                              size_t buf_sz, size_t *out_len);
int chaseros_ext2_read_file(const chaseros_vol_t *v, const char *name, char *buf, size_t buf_sz,
                        size_t *out_len);
int chaseros_ext2_write_file(const chaseros_vol_t *v, const char *name, const char *data, size_t len);
/** 创建目录（绝对路径，如 "/a" 或 "/a/b"；父路径须已存在） */
int chaseros_ext2_mkdir(const chaseros_vol_t *v, const char *path);
/** path 已规范化 */
int chaseros_ext2_path_is_dir(const chaseros_vol_t *v, const char *norm_path);

#endif

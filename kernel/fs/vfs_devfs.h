/* vfs_devfs.h — 伪设备后端（/dev/null、/dev/zero 等），与 ext2 根卷并存 */

#ifndef KERNEL_FS_VFS_DEVFS_H
#define KERNEL_FS_VFS_DEVFS_H

#include "fs/vfs.h"
#include <stddef.h>

int vfs_devfs_handles(const char *norm_path);

int vfs_devfs_ls(const char *norm_path);
int vfs_devfs_stat(const char *norm_path, vfs_stat_t *st);
int vfs_devfs_is_directory(const char *norm_path);
int vfs_devfs_read_range(const char *norm_path, uint32_t offset, void *buf, size_t buf_sz,
                         size_t *out_len);
int vfs_devfs_write_file(const char *norm_path, const char *data, size_t len);
int vfs_devfs_mkdir(const char *norm_path);

#endif

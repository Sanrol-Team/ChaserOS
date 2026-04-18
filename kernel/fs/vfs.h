/* kernel/fs/vfs.h - 虚拟文件系统接口（预留，具体 FS 后续实现）
 * e2fsprogs：仓库内为上游源码树，CNOS 仅使用其中一部分；说明见 fs/README。
 * CNAF/CNAFL：fs/cnaf/cnaf.h、fs/cnaf/cnaf_spec.h */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_ERR_NONE     0
#define VFS_ERR_NOENT   -1
#define VFS_ERR_NOSYS   -2

typedef uint64_t vfs_ino_t;

typedef struct vfs_stat {
    vfs_ino_t ino;
    uint64_t size;
} vfs_stat_t;

void vfs_init(void);

/* 根卷挂载占位；返回 VFS_ERR_NOSYS 直至实现具体 FS */
int vfs_mount_root(void);

#endif

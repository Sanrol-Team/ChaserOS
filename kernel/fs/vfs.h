/* kernel/fs/vfs.h - 虚拟文件系统：根挂载 + 多后端（ext2 块卷 + /dev 伪节点等） */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_ERR_NONE         0
#define VFS_ERR_NOENT       -1 /* 无块设备卷，或底层「未找到」（保留与 FS 共用） */
#define VFS_ERR_NOSYS       -2 /* 未实现 */
#define VFS_ERR_NOTMOUNTED  -3 /* 根未挂载或卷已分离 */
#define VFS_ERR_IO          -4 /* 后端 I/O 或格式错误 */

typedef uint64_t vfs_ino_t;

typedef struct vfs_stat {
    vfs_ino_t ino;
    uint64_t size;
} vfs_stat_t;

typedef enum {
    VFS_FS_NONE = 0,
    VFS_FS_EXT2, /* 根卷块设备上的 ext2 */
    VFS_FS_DEV,  /* 内核伪节点（见 vfs_devfs），与 ext2 并存 */
} vfs_fstype_t;

void vfs_init(void);

/* 将当前 chaseros_vol 挂为根（读目录、读写文件前须成功挂载） */
int vfs_mount_root(void);
int vfs_umount_root(void);
int vfs_is_mounted(void);
vfs_fstype_t vfs_root_fstype(void);

/* 在当前卷上创建 ext2（不要求已 mount；破坏原有数据） */
int vfs_format(void);

/** path 为 NULL 或 "" 时列出根目录 "/" */
int vfs_ls(const char *path);
int vfs_mkdir(const char *path);
/** 相对 cwd 拼出绝对路径并规范化（输出以 '/' 开头） */
int vfs_resolve_path(const char *cwd, const char *rel, char *out, size_t outsz);
/** 已挂载且 path 为规范化后的目录则返回非零 */
int vfs_is_directory(const char *path);
int vfs_read_file(const char *name, char *buf, size_t buf_sz, size_t *out_len);
/** 从普通文件偏移 offset 起读取（内核缓冲区 buf） */
int vfs_read_file_range(const char *name, uint32_t offset, void *buf, size_t buf_sz,
                        size_t *out_len);
int vfs_write_file(const char *name, const char *data, size_t len);

int vfs_stat(const char *path, vfs_stat_t *st);

#endif

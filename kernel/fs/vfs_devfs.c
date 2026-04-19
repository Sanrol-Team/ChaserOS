/* vfs_devfs.c — 最小 /dev 伪文件系统（不占用块设备） */

#include "fs/vfs_devfs.h"
#include <stdint.h>
#include <stddef.h>

extern void puts(const char *s);

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int is_under_dev(const char *norm) {
    if (!norm || norm[0] != '/' || norm[1] != 'd' || norm[2] != 'e' || norm[3] != 'v') {
        return 0;
    }
    return norm[4] == '\0' || norm[4] == '/';
}

int vfs_devfs_handles(const char *norm_path) {
    return is_under_dev(norm_path);
}

int vfs_devfs_is_directory(const char *norm_path) {
    return str_eq(norm_path, "/dev");
}

int vfs_devfs_stat(const char *norm_path, vfs_stat_t *st) {
    if (!st) {
        return VFS_ERR_IO;
    }
    if (str_eq(norm_path, "/dev")) {
        st->ino = 1;
        st->size = 0;
        return VFS_ERR_NONE;
    }
    if (str_eq(norm_path, "/dev/null")) {
        st->ino = 2;
        st->size = 0;
        return VFS_ERR_NONE;
    }
    if (str_eq(norm_path, "/dev/zero")) {
        st->ino = 3;
        st->size = 0xFFFFFFFFull;
        return VFS_ERR_NONE;
    }
    return VFS_ERR_NOENT;
}

int vfs_devfs_ls(const char *norm_path) {
    if (!str_eq(norm_path, "/dev")) {
        return VFS_ERR_IO;
    }
    puts("  null  ino=2\n");
    puts("  zero  ino=3\n");
    return VFS_ERR_NONE;
}

int vfs_devfs_read_range(const char *norm_path, uint32_t offset, void *buf, size_t buf_sz,
                         size_t *out_len) {
    if (!buf || !out_len) {
        return VFS_ERR_IO;
    }
    if (str_eq(norm_path, "/dev/null")) {
        *out_len = 0;
        (void)offset;
        return VFS_ERR_NONE;
    }
    if (str_eq(norm_path, "/dev/zero")) {
        size_t i;
        uint8_t *b = (uint8_t *)buf;
        for (i = 0; i < buf_sz; i++) {
            b[i] = 0;
        }
        *out_len = buf_sz;
        (void)offset;
        return VFS_ERR_NONE;
    }
    return VFS_ERR_NOENT;
}

int vfs_devfs_write_file(const char *norm_path, const char *data, size_t len) {
    (void)data;
    (void)len;
    if (str_eq(norm_path, "/dev/null")) {
        return VFS_ERR_NONE;
    }
    if (str_eq(norm_path, "/dev/zero")) {
        return VFS_ERR_IO;
    }
    return VFS_ERR_NOENT;
}

int vfs_devfs_mkdir(const char *norm_path) {
    (void)norm_path;
    return VFS_ERR_IO;
}

/* kernel/fs/vfs.c - VFS 实现：根挂载位 + 多后端（ext2 块卷 + /dev 伪 FS） */

#include "fs/vfs.h"
#include "fs/vfs_devfs.h"
#include "fs/chaseros/porting.h"
#include "fs/chaseros/chaseros_ext2_vol.h"
#include "user_fd.h"

typedef struct {
    int mounted;
    vfs_fstype_t fstype;
} vfs_root_state_t;

static vfs_root_state_t g_root;

static const chaseros_vol_t *vfs_active_vol(void) {
    if (!g_root.mounted) {
        return NULL;
    }
    return chaseros_vol_current();
}

void vfs_init(void) {
    g_root.mounted = 0;
    g_root.fstype = VFS_FS_NONE;
    chaseros_ext2_init();
}

int vfs_mount_root(void) {
    const chaseros_vol_t *v = chaseros_vol_current();
    if (!v) {
        return VFS_ERR_NOENT;
    }
    g_root.mounted = 1;
    g_root.fstype = VFS_FS_EXT2;
    return VFS_ERR_NONE;
}

int vfs_umount_root(void) {
    user_fd_umount_close_all();
    g_root.mounted = 0;
    g_root.fstype = VFS_FS_NONE;
    return VFS_ERR_NONE;
}

int vfs_is_mounted(void) {
    return g_root.mounted && chaseros_vol_current() != NULL;
}

vfs_fstype_t vfs_root_fstype(void) {
    return g_root.fstype;
}

int vfs_format(void) {
    const chaseros_vol_t *v = chaseros_vol_current();
    if (!v) {
        return VFS_ERR_NOENT;
    }
    if (chaseros_ext2_format(v) != 0) {
        return VFS_ERR_IO;
    }
    return VFS_ERR_NONE;
}

int vfs_resolve_path(const char *cwd, const char *rel, char *out, size_t outsz) {
    char tmp[512];
    size_t ci = 0;
    size_t ri = 0;
    if (!cwd || !rel || !out || outsz < 2u) {
        return -1;
    }
    while (cwd[ci]) {
        ci++;
    }
    while (rel[ri]) {
        ri++;
    }
    if (rel[0] == '/') {
        return chaseros_path_normalize(rel, out, outsz);
    }
    if (ci + 1u + ri + 2u > sizeof tmp) {
        return -1;
    }
    if (ci == 1u && cwd[0] == '/') {
        tmp[0] = '/';
        size_t k;
        for (k = 0; k <= ri; k++) {
            tmp[1u + k] = rel[k];
        }
    } else {
        size_t k;
        for (k = 0; k < ci; k++) {
            tmp[k] = cwd[k];
        }
        tmp[ci] = '/';
        for (k = 0; k <= ri; k++) {
            tmp[ci + 1u + k] = rel[k];
        }
    }
    return chaseros_path_normalize(tmp, out, outsz);
}

int vfs_ls(const char *path) {
    const chaseros_vol_t *v = vfs_active_vol();
    if (!v) {
        return VFS_ERR_NOTMOUNTED;
    }
    const char *p = path;
    if (!p || p[0] == '\0') {
        p = "/";
    }
    char norm[256];
    if (chaseros_path_normalize(p, norm, sizeof norm) != 0) {
        return VFS_ERR_IO;
    }
    if (vfs_devfs_handles(norm)) {
        return vfs_devfs_ls(norm);
    }
    if (chaseros_ext2_ls_at(v, norm) != 0) {
        return VFS_ERR_IO;
    }
    return VFS_ERR_NONE;
}

int vfs_mkdir(const char *path) {
    const chaseros_vol_t *v = vfs_active_vol();
    if (!v) {
        return VFS_ERR_NOTMOUNTED;
    }
    char norm[256];
    if (chaseros_path_normalize(path, norm, sizeof norm) != 0) {
        return VFS_ERR_IO;
    }
    if (vfs_devfs_handles(norm)) {
        return vfs_devfs_mkdir(norm);
    }
    if (chaseros_ext2_mkdir(v, norm) != 0) {
        return VFS_ERR_IO;
    }
    return VFS_ERR_NONE;
}

int vfs_is_directory(const char *path) {
    const chaseros_vol_t *v = vfs_active_vol();
    if (!v) {
        return 0;
    }
    char norm[256];
    if (chaseros_path_normalize(path, norm, sizeof norm) != 0) {
        return 0;
    }
    if (vfs_devfs_handles(norm)) {
        return vfs_devfs_is_directory(norm);
    }
    return chaseros_ext2_path_is_dir(v, norm);
}

int vfs_read_file_range(const char *name, uint32_t offset, void *buf, size_t buf_sz,
                        size_t *out_len) {
    const chaseros_vol_t *v = vfs_active_vol();
    if (!v) {
        return VFS_ERR_NOTMOUNTED;
    }
    char norm[256];
    if (chaseros_path_normalize(name, norm, sizeof norm) != 0) {
        return VFS_ERR_IO;
    }
    if (vfs_devfs_handles(norm)) {
        return vfs_devfs_read_range(norm, offset, buf, buf_sz, out_len);
    }
    if (chaseros_ext2_read_file_range(v, norm, offset, buf, buf_sz, out_len) != 0) {
        return VFS_ERR_IO;
    }
    return VFS_ERR_NONE;
}

int vfs_read_file(const char *name, char *buf, size_t buf_sz, size_t *out_len) {
    return vfs_read_file_range(name, 0u, buf, buf_sz, out_len);
}

int vfs_write_file(const char *name, const char *data, size_t len) {
    const chaseros_vol_t *v = vfs_active_vol();
    if (!v) {
        return VFS_ERR_NOTMOUNTED;
    }
    char norm[256];
    if (chaseros_path_normalize(name, norm, sizeof norm) != 0) {
        return VFS_ERR_IO;
    }
    if (vfs_devfs_handles(norm)) {
        return vfs_devfs_write_file(norm, data, len);
    }
    if (chaseros_ext2_write_file(v, norm, data, len) != 0) {
        return VFS_ERR_IO;
    }
    return VFS_ERR_NONE;
}

int vfs_stat(const char *path, vfs_stat_t *st) {
    const chaseros_vol_t *v = vfs_active_vol();
    if (!v) {
        return VFS_ERR_NOTMOUNTED;
    }
    if (!path || !st) {
        return VFS_ERR_IO;
    }
    char norm[256];
    if (chaseros_path_normalize(path, norm, sizeof norm) != 0) {
        return VFS_ERR_IO;
    }
    if (vfs_devfs_handles(norm)) {
        return vfs_devfs_stat(norm, st);
    }
    uint32_t sz = 0;
    if (chaseros_ext2_stat_file(v, norm, &sz) != 0) {
        return VFS_ERR_NOENT;
    }
    st->ino = 0;
    st->size = sz;
    return VFS_ERR_NONE;
}

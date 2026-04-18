/* kernel/fs/vfs.c */

#include "fs/vfs.h"
#include "fs/cnos/porting.h"

void vfs_init(void) {
    cnos_ext2_init();
}

int vfs_mount_root(void) {
    return VFS_ERR_NOSYS;
}

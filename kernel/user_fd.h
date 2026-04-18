/* Per-embedded-user 文件描述符表（根目录下普通文件路径 + 读游标） */

#ifndef KERNEL_USER_FD_H
#define KERNEL_USER_FD_H

#include <stddef.h>
#include <stdint.h>

void user_fd_reset(void);
void user_fd_umount_close_all(void);

/** 内核路径字符串打开（SYS_OPEN 与 IPC FS 服务共用实现） */
long user_fd_open_kpath(const char *path, int flags);

long user_fd_sys_open(uint64_t path_user_ptr, int flags);
long user_fd_sys_close(int fd);
long user_fd_sys_read(int fd, uint64_t buf_user_ptr, size_t len);

#endif

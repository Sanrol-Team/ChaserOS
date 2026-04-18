/* kernel/errno.h — 用户态可见负返回值 -errno（与 POSIX 编号对齐，子集） */

#ifndef KERNEL_ERRNO_H
#define KERNEL_ERRNO_H

#define EPERM   1
#define EBADF   9
#define EINVAL  22
#define EFAULT  14
#define ENOSYS  38

#endif

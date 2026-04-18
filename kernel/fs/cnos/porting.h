/*
 * CNOS 与 e2fsprogs/libext2fs 的适配说明（上游源码在 kernel/fs/lib 等目录）
 *
 * kernel/fs 里虽有整棵 e2fsprogs 参考树，CNOS **只用其中一部分**源码（按 CMake 逐项加入
 * libext2fs 的 .c）；多数文件不参与链接。总览见 kernel/fs/README。
 *
 * 目标：只“借用” ext2/3/4 的核心读写与 on-disk 算法，不整套搬进用户态库。
 *
 * 要拿的（lib/ext2fs 中按需裁剪）：
 *  - 超级块 / 组描述符 / 位图 / inode / 目录 / extent（ext4）等布局与解析、分配与更新路径；
 *  - 与上述直接相关的块级读写编排（经 io_channel / io_manager 抽象）。
 *
 * 不要或需替换的：
 *  - unix_io、stdio、open/read/write、mmap、路径名、信号、profile 等 POSIX 外壳；
 *  - 内存：全部走 cnos_malloc / cnos_free（或后续页分配器），禁止隐式 libc heap；
 *  - I/O：实现 CNOS 专用 io_manager（内存盘或块设备），不链接 unix_io；
 *  - 线程与锁：内核单线程；不编 HAVE_PTHREAD 路径；若上游文件含 mutex，用 stubs 或
 *    本地无锁副本（cnos_io 从零写，不复制 unix_io 的 pthread 分支）。
 *
 * 明确不链接的目录：
 *  - misc/、e2fsck/、resize/、tests/、util/、fuse2fs 等用户态工具与测试。
 *
 * 接入顺序建议：
 *  1. cnos_io：struct_io_manager（read_blk / write_blk / set_blksize / close），对接 RAM/块设备。
 *  2. cnos_alloc：.fs_reserve bump 或后续可换正式分配器。
 *  3. config.h：替代 autoconf；见 HAVE_PTHREAD 等开关。
 *  4. ext2_types.h、ext2_err + 最小 com_err 或桩；再按依赖逐步加入 lib/ext2fs 下各 .c。
 *
 * 许可证：上游文件遵循其原有 GPL/LGPL；CNOS 新增文件按项目整体许可。
 */

#ifndef CNOS_FS_PORTING_H
#define CNOS_FS_PORTING_H

#define CNOS_FS_PORTING 1

#include <stdint.h>
#include <stddef.h>

void *cnos_malloc(size_t n);
void cnos_free(void *p);

void cnos_ext2_init(void);

#endif

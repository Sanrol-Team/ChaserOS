/*
 * 替代 e2fsprogs 的 autoconf config.h；随 libext2fs 逐步编入内核而扩充。
 * 在编译任何 lib/ext2fs 下 .c 前加入：-include kernel/fs/cnos/config.h
 * （或通过 CMake 目标级 FORCE_INCLUDE）。
 */

#ifndef CNOS_EXT2_CONFIG_H
#define CNOS_EXT2_CONFIG_H

#define CNOS_KERNEL 1

/*
 * 单线程：切勿 #define HAVE_PTHREAD（哪怕为 0），否则上游里 #ifdef HAVE_PTHREAD
 * 仍为真，会拉进 pthread。正确做法是保持 HAVE_PTHREAD 未定义。
 */

#define HAVE_STDLIB_H 0
#define HAVE_STRING_H 1
#define HAVE_ERRNO_H 0
#define HAVE_UNISTD_H 0
#define HAVE_FCNTL_H 0
#define HAVE_SYS_TYPES_H 0
#define HAVE_SYS_STAT_H 0
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 0

#define malloc cnos_malloc
#define free cnos_free

#endif

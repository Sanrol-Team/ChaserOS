#!/bin/bash
set -e

PREFIX="$1"
TARGET=x86_64-elf
BINUTILS_VER=2.42
GCC_VER=14.2.0 # 建议用最新的 14.2.0

# 创建临时构建目录
SRCDIR="$(mkdir -p build-tmp && cd build-tmp && pwd)"
cd "$SRCDIR"

echo "--- Building toolchain in $SRCDIR, Installing to $PREFIX ---"

# 1. 下载源码
wget -q https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.gz
wget -q https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.gz

tar -xf binutils-$BINUTILS_VER.tar.gz
tar -xf gcc-$GCC_VER.tar.gz

# 2. 编译 Binutils
mkdir -p build-binutils && cd build-binutils
../binutils-$BINUTILS_VER/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror
make -j$(nproc)
make install
cd ..

# 3. 编译 GCC (仅 C/C++ 核心)
mkdir -p build-gcc && cd build-gcc
../gcc-$GCC_VER/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers \
    --with-gnu-as \
    --with-gnu-ld
    
make -j$(nproc) all-gcc
make install-gcc

# 4. 编译 Libgcc (内核开发必需)
make -j$(nproc) all-target-libgcc
make install-target-libgcc

echo "--- Toolchain Build Complete ---"
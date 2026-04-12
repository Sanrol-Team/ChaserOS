#!/bin/bash
# 从 GNU 源码构建 x86_64-elf 交叉 binutils + gcc（裸机 / 无 libc）。
set -e

TARGET=x86_64-elf
BINUTILS_VER=2.42
GCC_VER=14.2.0
GNU_MIRROR="${GNU_MIRROR:-https://ftp.gnu.org/gnu}"

if [ -z "${1:-}" ]; then
  echo "用法: $0 <安装前缀>" >&2
  exit 1
fi

PREFIX="$1"
mkdir -p "$PREFIX"
# 获取绝对路径，防止 configure 阶段路径解析出错
PREFIX="$(cd "$PREFIX" && pwd)"

# 检查是否已安装（用于 CI 缓存逻辑）
if [ -x "$PREFIX/bin/${TARGET}-gcc" ] && [ -x "$PREFIX/bin/${TARGET}-ld" ]; then
  echo "已存在 ${TARGET}-gcc / ${TARGET}-ld（$PREFIX），跳过编译。"
  exit 0
fi

ROOT="$(pwd)"
SRCDIR="$ROOT/build-tmp"
# 确保构建目录干净
rm -rf "$SRCDIR"
mkdir -p "$SRCDIR"
cd "$SRCDIR"

fetch() {
  local url="$1" out="$2"
  echo "正在下载: $url"
  if command -v wget >/dev/null 2>&1; then
    wget -q --show-progress -O "$out" "$url"
  else
    curl -fsSL -o "$out" "$url"
  fi
}

echo "=== 构建目录: $SRCDIR，安装到: $PREFIX ==="

# 下载源码包（使用 .xz 格式体积更小，下载更快）
fetch "$GNU_MIRROR/binutils/binutils-${BINUTILS_VER}.tar.xz" "binutils-${BINUTILS_VER}.tar.xz"
fetch "$GNU_MIRROR/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz" "gcc-${GCC_VER}.tar.xz"

echo "--- 正在解压 ---"
tar -xf "binutils-${BINUTILS_VER}.tar.xz"
tar -xf "gcc-${GCC_VER}.tar.xz"

# --- 1. 编译 binutils ---
echo "--- 正在编译 Binutils ---"
mkdir -p build-binutils && cd build-binutils
../binutils-${BINUTILS_VER}/configure \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --with-sysroot \
  --disable-nls \
  --disable-werror
make -j"$(nproc)"
make install
cd "$SRCDIR"

# 必须将新编译的 binutils 加入 PATH，否则 GCC configure 可能会找不到交叉汇编器
export PATH="$PREFIX/bin:$PATH"

# --- 2. 准备 GCC 依赖 ---
echo "--- 正在准备 GCC 依赖 (GMP/MPFR/MPC) ---"
cd "gcc-${GCC_VER}"
# 该脚本会自动下载源码并解压到当前目录，GCC configure 会自动识别并内建编译
./contrib/download_prerequisites
cd "$SRCDIR"

# --- 3. 编译 GCC ---
echo "--- 正在编译 GCC ---"
mkdir -p build-gcc && cd build-gcc
../gcc-${GCC_VER}/configure \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --disable-nls \
  --enable-languages=c,c++ \
  --without-headers \
  --disable-multilib \
  --disable-threads \
  --with-gnu-as \
  --with-gnu-ld

# all-gcc 编译编译器核心, all-target-libgcc 编译运行时支持库（处理 64 位数学运算等）
make -j"$(nproc)" all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd "$SRCDIR"

# --- 清理 ---
cd "$ROOT"
rm -rf "$SRCDIR"

echo "=== 工具链安装成功！位置: $PREFIX ==="
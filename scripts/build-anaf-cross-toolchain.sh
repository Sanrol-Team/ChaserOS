#!/usr/bin/env bash
#
# ANAF 交叉编译工具链（GNU Binutils + GCC，裸机、无 libc）
#
# ANAF = Auxiliary Native Application Freestanding（CNOS 工程内对「裸机交叉前缀」的命名）
#
# 默认目标与 CNOS 内核一致：x86_64-elf。可通过环境变量覆盖。
#
# 用法：
#   bash scripts/build-anaf-cross-toolchain.sh <安装前缀>
#
# 可选环境变量：
#   ANAF_TARGET / ANAF_GNU_TARGET   目标三元组，默认 x86_64-elf
#   ANAF_BINUTILS_VER               默认 2.42
#   ANAF_GCC_VER                    默认 14.2.0
#   GNU_MIRROR                      GNU 镜像根 URL
#
set -euo pipefail

TARGET="${ANAF_TARGET:-${ANAF_GNU_TARGET:-x86_64-elf}}"
BINUTILS_VER="${ANAF_BINUTILS_VER:-2.42}"
GCC_VER="${ANAF_GCC_VER:-14.2.0}"
GNU_MIRROR="${GNU_MIRROR:-https://ftp.gnu.org/gnu}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -z "${1:-}" ]; then
  echo "用法: $0 <安装前缀>" >&2
  echo "当前 ANAF_TARGET=${TARGET}" >&2
  exit 1
fi

PREFIX="$1"
mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd)"

if [ -x "$PREFIX/bin/${TARGET}-gcc" ] && [ -x "$PREFIX/bin/${TARGET}-ld" ]; then
  echo "ANAF: 已存在 ${TARGET}-gcc / ${TARGET}-ld（$PREFIX），跳过编译。"
  cat >"$PREFIX/anaf-toolchain.env" <<EOF
# 由 scripts/build-anaf-cross-toolchain.sh 生成（已存在编译器时仅写 env）
export ANAF_PREFIX="$PREFIX"
export ANAF_GNU_TARGET="$TARGET"
export PATH="$PREFIX/bin:\$PATH"
EOF
  echo "已写入: $PREFIX/anaf-toolchain.env"
  exit 0
fi

SRCDIR="$REPO_ROOT/build-tmp-anaf-${TARGET}"
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

echo "=== ANAF 交叉工具链 ==="
echo "    目标: $TARGET"
echo "    Binutils: $BINUTILS_VER  GCC: $GCC_VER"
echo "    构建目录: $SRCDIR"
echo "    安装前缀: $PREFIX"
echo "========================"

fetch "$GNU_MIRROR/binutils/binutils-${BINUTILS_VER}.tar.xz" "binutils-${BINUTILS_VER}.tar.xz"
fetch "$GNU_MIRROR/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz" "gcc-${GCC_VER}.tar.xz"

echo "--- 正在解压 ---"
tar -xf "binutils-${BINUTILS_VER}.tar.xz"
tar -xf "gcc-${GCC_VER}.tar.xz"

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

export PATH="$PREFIX/bin:$PATH"

echo "--- 正在准备 GCC 依赖 (GMP/MPFR/MPC) ---"
cd "gcc-${GCC_VER}"
./contrib/download_prerequisites
cd "$SRCDIR"

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

make -j"$(nproc)" all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd "$SRCDIR"

cd "$REPO_ROOT"
rm -rf "$SRCDIR"

cat >"$PREFIX/anaf-toolchain.env" <<EOF
# 由 scripts/build-anaf-cross-toolchain.sh 生成 — 使用前: source <此文件>
export ANAF_PREFIX="$PREFIX"
export ANAF_GNU_TARGET="$TARGET"
export PATH="$PREFIX/bin:\$PATH"
EOF

echo "=== ANAF 工具链安装完成: $PREFIX ==="
echo "    执行: source $PREFIX/anaf-toolchain.env"
echo "    CMake: cmake -B build -DCMAKE_TOOLCHAIN_FILE=$REPO_ROOT/cmake/anaf.cmake"

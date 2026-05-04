#!/usr/bin/env bash
#
# ChaserOS Native GCC（阶段 1 引导链）
#
# 目标：
#   构建目标三元组 x86_64-chaseros 的 GNU Binutils + GCC（freestanding, without headers），
#   作为后续“可在 ChaserOS 上原生运行 GCC”的前置开发工具链。
#
# 说明：
#   1) 这是原生编译器开发的 bootstrap 阶段，不是完整 hosted 工具链。
#   2) 当前仍生成交叉工具（在宿主 Linux 运行），后续逐步接入 chaseros libc/sysroot。
#
# 用法：
#   bash scripts/build-chaseros-native-gcc.sh <安装前缀>
#
# 可选环境变量：
#   CHASEROS_NATIVE_TARGET        默认 x86_64-chaseros
#   CHASEROS_NATIVE_BINUTILS_VER  默认 2.42
#   CHASEROS_NATIVE_GCC_VER       默认 14.2.0
#   CHASEROS_NATIVE_USE_LOCAL_ELF 默认 auto；为 1/true 时优先复用本地 x86_64-elf-*
#   CHASEROS_LOCAL_ELF_PREFIX     指定本地 x86_64-elf 前缀（例如 $HOME/opt/cross）
#
set -euo pipefail

if [ -z "${1:-}" ]; then
  echo "用法: $0 <安装前缀>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PREFIX="$1"
mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd)"

TARGET="${CHASEROS_NATIVE_TARGET:-x86_64-chaseros}"
BINUTILS_VER="${CHASEROS_NATIVE_BINUTILS_VER:-2.42}"
GCC_VER="${CHASEROS_NATIVE_GCC_VER:-14.2.0}"

echo "============================================================"
echo " ChaserOS Native GCC bootstrap"
echo "  target : $TARGET"
echo "  prefix : $PREFIX"
echo "  gcc    : $GCC_VER"
echo "  binutils: $BINUTILS_VER"
echo "============================================================"

USE_LOCAL_ELF="${CHASEROS_NATIVE_USE_LOCAL_ELF:-auto}"
LOCAL_ELF_PREFIX="${CHASEROS_LOCAL_ELF_PREFIX:-}"
REUSED_LOCAL_ELF=0
REAL_GCC=""

if [ "$TARGET" = "x86_64-chaseros" ]; then
  if [ -n "$LOCAL_ELF_PREFIX" ] && [ -x "$LOCAL_ELF_PREFIX/bin/x86_64-elf-gcc" ]; then
    REUSED_LOCAL_ELF=1
    REAL_GCC="$LOCAL_ELF_PREFIX/bin/x86_64-elf-gcc"
    install -d "$PREFIX/bin"
    for t in gcc g++ ld ar ranlib objcopy; do
      cat >"$PREFIX/bin/$TARGET-$t" <<EOF
#!/usr/bin/env bash
exec "$LOCAL_ELF_PREFIX/bin/x86_64-elf-$t" "\$@"
EOF
      chmod 0755 "$PREFIX/bin/$TARGET-$t"
    done
  elif { [ "$USE_LOCAL_ELF" = "1" ] || [ "$USE_LOCAL_ELF" = "true" ] || [ "$USE_LOCAL_ELF" = "auto" ]; } \
       && command -v x86_64-elf-gcc >/dev/null 2>&1; then
    REUSED_LOCAL_ELF=1
    REAL_GCC="$(command -v x86_64-elf-gcc)"
    install -d "$PREFIX/bin"
    for t in gcc g++ ld ar ranlib objcopy; do
      cat >"$PREFIX/bin/$TARGET-$t" <<EOF
#!/usr/bin/env bash
exec "$(command -v x86_64-elf-$t)" "\$@"
EOF
      chmod 0755 "$PREFIX/bin/$TARGET-$t"
    done
  fi
fi

if [ "$REUSED_LOCAL_ELF" -eq 1 ]; then
  echo "using local x86_64-elf toolchain via wrapper: $TARGET-* -> x86_64-elf-*"
else
  export ANAF_TARGET="$TARGET"
  export ANAF_BINUTILS_VER="$BINUTILS_VER"
  export ANAF_GCC_VER="$GCC_VER"
  bash "$ROOT/scripts/build-anaf-cross-toolchain.sh" "$PREFIX"
fi

install -d "$PREFIX/$TARGET-sysroot"
install -d "$PREFIX/$TARGET-sysroot/usr/include"
install -d "$PREFIX/$TARGET-sysroot/usr/lib"
install -d "$PREFIX/$TARGET-sysroot/usr/lib/ldscripts"
install -d "$PREFIX/$TARGET-sysroot/usr/include/chaseros"
install -d "$PREFIX/share/chaseros/cmake"
install -d "$PREFIX/share/chaseros/sysroot/include"
install -d "$PREFIX/share/chaseros/runtime"

if [ -f "$ROOT/kernel/fs/cnaf/cnaf_spec.h" ]; then
  install -m0644 "$ROOT/kernel/fs/cnaf/cnaf_spec.h" "$PREFIX/share/chaseros/sysroot/include/cnaf_spec.h"
fi

if [ -f "$ROOT/user/include/chaseros_user.h" ]; then
  install -m0644 "$ROOT/user/include/chaseros_user.h" "$PREFIX/$TARGET-sysroot/usr/include/chaseros/chaseros_user.h"
fi
if [ -f "$ROOT/kernel/syscall_abi.h" ]; then
  install -m0644 "$ROOT/kernel/syscall_abi.h" "$PREFIX/$TARGET-sysroot/usr/include/chaseros/syscall_abi.h"
fi
if [ -f "$ROOT/toolchains/gcc-for-chaseros/runtime/include/chaseros_user_runtime.h" ]; then
  install -m0644 "$ROOT/toolchains/gcc-for-chaseros/runtime/include/chaseros_user_runtime.h" \
    "$PREFIX/$TARGET-sysroot/usr/include/chaseros/chaseros_user_runtime.h"
fi

if [ -f "$ROOT/user/user.ld" ]; then
  install -m0644 "$ROOT/user/user.ld" "$PREFIX/$TARGET-sysroot/usr/lib/ldscripts/user.ld"
fi

if [ -f "$ROOT/cmake/x86_64-chaseros-gcc.cmake" ]; then
  install -m0644 "$ROOT/cmake/x86_64-chaseros-gcc.cmake" "$PREFIX/share/chaseros/cmake/x86_64-chaseros-gcc.cmake"
fi

CRT0_SRC="$ROOT/toolchains/gcc-for-chaseros/runtime/crt0.S"
RUNTIME_C="$ROOT/toolchains/gcc-for-chaseros/runtime/chaseros_user_runtime.c"
if [ -f "$CRT0_SRC" ] && [ -f "$RUNTIME_C" ]; then
  TMPDIR="$ROOT/build-tmp-chaseros-runtime-$TARGET"
  rm -rf "$TMPDIR"
  mkdir -p "$TMPDIR"
  "$PREFIX/bin/$TARGET-gcc" -ffreestanding -c "$CRT0_SRC" -o "$TMPDIR/crt0.o"
  "$PREFIX/bin/$TARGET-gcc" -ffreestanding -O2 -Wall -Wextra -c "$RUNTIME_C" -o "$TMPDIR/chaseros_user_runtime.o"
  "$PREFIX/bin/$TARGET-ar" rcs "$TMPDIR/libchaseros_user.a" "$TMPDIR/chaseros_user_runtime.o"
  install -m0644 "$TMPDIR/crt0.o" "$PREFIX/$TARGET-sysroot/usr/lib/crt0.o"
  install -m0644 "$TMPDIR/libchaseros_user.a" "$PREFIX/$TARGET-sysroot/usr/lib/libchaseros_user.a"
  install -m0644 "$TMPDIR/libchaseros_user.a" "$PREFIX/share/chaseros/runtime/libchaseros_user.a"
  rm -rf "$TMPDIR"
fi

cat >"$PREFIX/bin/chaseros-native-gcc-env.sh" <<EOF
#!/usr/bin/env bash
# ChaserOS Native GCC 环境（由 build-chaseros-native-gcc.sh 生成）
_ROOT="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")/.." && pwd)"
export CHASEROS_NATIVE_GCC_ROOT="\$_ROOT"
export CHASEROS_NATIVE_TARGET="$TARGET"
export CHASEROS_SYSROOT="\$_ROOT/$TARGET-sysroot"
export CHASEROS_REAL_GCC="$REAL_GCC"
export PATH="\$_ROOT/bin:\$PATH"
export CC="$TARGET-gcc"
export CXX="$TARGET-g++"
export AR="$TARGET-ar"
export LD="$TARGET-ld"
export OBJCOPY="$TARGET-objcopy"
export CMAKE_TOOLCHAIN_FILE="\$_ROOT/share/chaseros/cmake/x86_64-chaseros-gcc.cmake"
export CHASEROS_USER_LD="\$CHASEROS_SYSROOT/usr/lib/ldscripts/user.ld"
export CHASEROS_USER_CFLAGS="-ffreestanding -fno-stack-protector -isystem \$CHASEROS_SYSROOT/usr/include"
export CHASEROS_USER_LDFLAGS="-nostdlib -Wl,-T,\$CHASEROS_USER_LD -Wl,--gc-sections"
if [ -n "\${CHASEROS_REAL_GCC:-}" ]; then
  _lg="\$("\$CHASEROS_REAL_GCC" -print-libgcc-file-name)"
  if [ "\$_lg" = "libgcc.a" ]; then
    _install_dir="\$("\$CHASEROS_REAL_GCC" -print-search-dirs | awk -F': ' '/^install: /{print \$2; exit}')"
    _lg="\${_install_dir}libgcc.a"
  fi
  export CHASEROS_LIBGCC_FILE="\$_lg"
else
  _lg="\$($TARGET-gcc -print-libgcc-file-name)"
  if [ "\$_lg" = "libgcc.a" ]; then
    _install_dir="\$($TARGET-gcc -print-search-dirs | awk -F': ' '/^install: /{print \$2; exit}')"
    _lg="\${_install_dir}libgcc.a"
  fi
  export CHASEROS_LIBGCC_FILE="\$_lg"
fi
export CHASEROS_USER_LIBS="-L\$CHASEROS_SYSROOT/usr/lib -lchaseros_user"
if [ -n "\${CHASEROS_LIBGCC_FILE:-}" ] && [ -f "\$CHASEROS_LIBGCC_FILE" ]; then
  export CHASEROS_USER_LIBS="\$CHASEROS_USER_LIBS \$CHASEROS_LIBGCC_FILE"
fi
export CHASEROS_USER_LINK_LINE="\$CC \$CHASEROS_USER_CFLAGS \$CHASEROS_USER_LDFLAGS \$CHASEROS_SYSROOT/usr/lib/crt0.o <obj...> \$CHASEROS_USER_LIBS -o <out.elf>"
EOF
chmod 0755 "$PREFIX/bin/chaseros-native-gcc-env.sh"

cat >"$PREFIX/share/chaseros/NATIVE-GCC-VERSION" <<EOF
name=ChaserOS Native GCC bootstrap
target=$TARGET
prefix=$PREFIX
gcc=$GCC_VER
binutils=$BINUTILS_VER
built=$(date -Iseconds 2>/dev/null || date)
EOF

echo ""
echo "完成。"
echo "  source $PREFIX/bin/chaseros-native-gcc-env.sh"
echo "  $TARGET-gcc --version"
echo ""

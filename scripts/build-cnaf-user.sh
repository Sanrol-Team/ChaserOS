#!/usr/bin/env bash
# 使用 ANAF（cmake/cnaf-anaf.cmake）单独构建 CNAF 用户态 ELF（供 .cnaf IMAGE 节）
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${CNOS_CNAF_USER_BUILD_DIR:-$ROOT/build-cnaf-user}"

if [ -f "${CNOS_APP_ROOT:-}/bin/cnos-app-env.sh" ]; then
  # shellcheck source=/dev/null
  source "${CNOS_APP_ROOT}/bin/cnos-app-env.sh"
elif [ -f "${ANAF_PREFIX:-}/anaf-toolchain.env" ]; then
  # shellcheck source=/dev/null
  source "${ANAF_PREFIX}/anaf-toolchain.env"
fi

echo "=== CNAF user (ANAF) ==="
echo "    Source: $ROOT/user"
echo "    Build:  $BUILD_DIR"
echo "    Toolchain file: $ROOT/cmake/cnaf-anaf.cmake"
echo "========================"

TF="${CMAKE_TOOLCHAIN_FILE:-${GCC_FOR_CNOS_CMAKE:-$ROOT/cmake/gcc-for-cnos.cmake}}"

cmake -S "$ROOT/user" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$TF" \
  "$@"

cmake --build "$BUILD_DIR"

echo "输出: $BUILD_DIR/hello.elf、$BUILD_DIR/minimal.elf（可嵌入 CNAF MANIFEST+IMAGE）"

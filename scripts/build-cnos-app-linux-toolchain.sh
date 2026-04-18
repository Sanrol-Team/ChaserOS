#!/usr/bin/env bash
#
# 在 Linux 宿主机上构建并安装「CNOS App」开发用 GCC 工具链（交叉目标 x86_64-elf）
#
# 组成：
#   1) GNU Binutils + GCC（与 scripts/build-anaf-cross-toolchain.sh 相同，ANAF）
#   2) CNAF 规范头文件 → $PREFIX/cnos-sysroot/include
#   3) CMake：anaf.cmake + cnaf-anaf.cmake → $PREFIX/share/cnos/cmake
#   4) 默认链接脚本 user.ld → $PREFIX/share/cnos/ld/
#   5) 环境脚本 bin/cnos-app-env.sh（source 后可用 CMAKE_TOOLCHAIN_FILE 与 SDK 路径）
#
# 用法：
#   bash scripts/build-cnos-app-linux-toolchain.sh <安装前缀>
#
# 示例：
#   bash scripts/build-cnos-app-linux-toolchain.sh "$HOME/opt/cnos-app"
#   source "$HOME/opt/cnos-app/bin/cnos-app-env.sh"
#   bash scripts/build-cnaf-user.sh
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

echo "=========================================="
echo " GCC For CNOS（Linux 宿主机 → x86_64-elf CNAF App）"
echo " 亦可称：CNOS App 捆绑工具链 / linux-cnos-app bundle"
echo " 安装前缀: $PREFIX"
echo "=========================================="

bash "$ROOT/scripts/build-anaf-cross-toolchain.sh" "$PREFIX"

install -d "$PREFIX/share/cnos/cmake"
install -d "$PREFIX/cnos-sysroot/include"
install -d "$PREFIX/share/cnos/ld"

install -m0644 "$ROOT/cmake/anaf.cmake" "$PREFIX/share/cnos/cmake/anaf.cmake"
install -m0644 "$ROOT/cmake/cnaf-anaf.cmake" "$PREFIX/share/cnos/cmake/cnaf-anaf.cmake"
install -m0644 "$ROOT/cmake/gcc-for-cnos.cmake" "$PREFIX/share/cnos/cmake/gcc-for-cnos.cmake"

for _h in cnaf.h cnaf_spec.h; do
  if [ -f "$ROOT/kernel/fs/cnaf/$_h" ]; then
    install -m0644 "$ROOT/kernel/fs/cnaf/$_h" "$PREFIX/cnos-sysroot/include/$_h"
  fi
done

install -m0644 "$ROOT/user/user.ld" "$PREFIX/share/cnos/ld/user.ld"

{
  echo "GCC For CNOS / CNOS App toolchain (Linux host)"
  echo "prefix=$PREFIX"
  echo "target=${ANAF_TARGET:-x86_64-elf}"
  echo "built=$(date -Iseconds 2>/dev/null || date)"
  echo "repo_cnaf_headers=kernel/fs/cnaf/*.h"
  echo "docs=toolchains/gcc-for-cnos/README.txt"
} >"$PREFIX/share/cnos/VERSION"

cat >"$PREFIX/bin/cnos-app-env.sh" <<'EOS'
#!/usr/bin/env bash
# CNOS App 工具链环境（由 build-cnos-app-linux-toolchain.sh 安装）
_TOOL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ -f "$_TOOL_ROOT/anaf-toolchain.env" ]; then
  # shellcheck source=/dev/null
  source "$_TOOL_ROOT/anaf-toolchain.env"
fi
export CNOS_APP_ROOT="$_TOOL_ROOT"
export CNOS_APP_TOOLCHAIN_ROOT="$_TOOL_ROOT"
export GCC_FOR_CNOS_ROOT="$_TOOL_ROOT"
export CMAKE_TOOLCHAIN_FILE="$_TOOL_ROOT/share/cnos/cmake/gcc-for-cnos.cmake"
export CNOS_APP_SDK_INCLUDE="$_TOOL_ROOT/cnos-sysroot/include"
export GCC_FOR_CNOS_CMAKE="$_TOOL_ROOT/share/cnos/cmake/gcc-for-cnos.cmake"
EOS
chmod 0755 "$PREFIX/bin/cnos-app-env.sh"

echo ""
echo "安装完成。"
echo "  使用前先执行:"
echo "    source $PREFIX/bin/cnos-app-env.sh"
echo "  然后编译 CNAF 用户 ELF，例如:"
echo "    bash $ROOT/scripts/build-cnaf-user.sh"
echo "  （或将 CNOS_CNAF_USER_BUILD_DIR 指向任意构建目录）"
echo ""

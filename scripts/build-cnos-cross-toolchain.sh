#!/usr/bin/env bash
#
# CNOS 交叉编译链 — 一键安装（Linux 宿主）
#
# 包含：
#   1) GCC For CNOS（x86_64-elf-gcc / ld）+ CNAF 头 + CMake 片段 + user.ld + cnos-app-env.sh
#   2) 可选：在本机用 cargo 构建 slimec（--target cnos）→ 安装到同一前缀 bin/
#
# 用法：
#   bash scripts/build-cnos-cross-toolchain.sh <安装前缀>
#   bash scripts/build-cnos-cross-toolchain.sh <安装前缀> --with-slime
#
# 完成后：
#   source <前缀>/bin/cnos-cross-env.sh
#
# 环境变量：见所生成脚本内注释；或与旧版仅需 GCC 时继续只用 cnos-app-env.sh。
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
  echo "用法: $0 <安装前缀> [--with-slime]" >&2
  echo "  --with-slime   在同一前缀安装 slimec（需 Rust/cargo 与 Slime 源码）" >&2
  exit 1
}

if [ -z "${1:-}" ] || [[ "${1:-}" == -* ]]; then
  usage
fi

mkdir -p "$1"
PREFIX="$(cd "$1" && pwd)"
WITH_SLIME=0
shift || true
for _a in "$@"; do
  case "$_a" in
    --with-slime) WITH_SLIME=1 ;;
    -h|--help) usage ;;
    *)
      echo "未知参数: $_a" >&2
      usage
      ;;
  esac
done

echo "============================================================"
echo " CNOS Cross Toolchain（GCC For CNOS"
if [ "$WITH_SLIME" -eq 1 ]; then echo " + slimec"; fi
echo ")"
echo "  PREFIX=$PREFIX"
echo "============================================================"

bash "$ROOT/scripts/build-cnos-app-linux-toolchain.sh" "$PREFIX"

SLIME_SRC="${SLIME_SRC:-$ROOT/integrations/slime-for-cnos/upstream/slime}"
if [ "$WITH_SLIME" -eq 1 ]; then
  if ! command -v cargo >/dev/null 2>&1; then
    echo "错误: --with-slime 需要 cargo（https://rustup.rs/）" >&2
    exit 1
  fi
  if [ ! -f "$SLIME_SRC/Cargo.toml" ]; then
    echo "错误: 未找到 Slime 源码 $SLIME_SRC（请先 bash scripts/fetch-slime-upstream.sh 或设置 SLIME_SRC）" >&2
    exit 1
  fi
  echo "--- cargo build slimec ---"
  ( cd "$SLIME_SRC" && cargo build --release )
  install -m0755 "$SLIME_SRC/target/release/slimec" "$PREFIX/bin/slimec"
  echo "已安装 slimec -> $PREFIX/bin/slimec"
fi

# 聚合环境：GCC +（可选）slime + 源码树路径
cat >"$PREFIX/bin/cnos-cross-env.sh" <<EOF
#!/usr/bin/env bash
# CNOS 交叉编译链 — 由 build-cnos-cross-toolchain.sh 生成；source 一次即可开发内核侧与用户态 CNAF/Slime
_CROSS="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")/.." && pwd)"
if [ -f "\$_CROSS/bin/cnos-app-env.sh" ]; then
  # shellcheck source=/dev/null
  source "\$_CROSS/bin/cnos-app-env.sh"
fi
export CNOS_CROSS_ROOT="\$_CROSS"
export CNOS_ROOT="$ROOT"
export PATH="\$_CROSS/bin:\${PATH}"
export CNOS_USER_LD="\${CNOS_USER_LD:-\$_CROSS/share/cnos/ld/user.ld}"
if [ -x "\$_CROSS/bin/slimec" ]; then
  export CNOS_SLIMEC="\$_CROSS/bin/slimec"
fi
EOF
chmod 0755 "$PREFIX/bin/cnos-cross-env.sh"

echo ""
echo "安装结束。"
echo "  source $PREFIX/bin/cnos-cross-env.sh"
echo ""
echo "  # C 用户 ELF:"
echo "  bash $ROOT/scripts/build-cnaf-user.sh"
echo ""
echo "  # Slime → ELF（需 slimec 含 cnos；若本次未 --with-slime，可自行安装后再 source）:"
echo "  bash $ROOT/scripts/cnos-slime-compile.sh $ROOT/user/hello.sm /tmp/a.elf"
echo ""
echo "  # Slime → CNAF:"
echo "  bash $ROOT/scripts/cnos-slimec.sh $ROOT/user/hello.sm /tmp/a.cnaf"
echo ""

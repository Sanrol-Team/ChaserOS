#!/usr/bin/env bash
# 构建 slimec 并安装到给定前缀，生成 cnos-slime-env.sh（Slime for CNOS 工具链）
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PREFIX="${1:?用法: $0 <安装前缀，如 \$HOME/opt/cnos-slime>}"

SLIME_SRC="${SLIME_SRC:-$ROOT/integrations/slime-for-cnos/upstream/slime}"

if [ ! -f "$SLIME_SRC/Cargo.toml" ]; then
  echo "未找到 Slime 源码: $SLIME_SRC"
  echo "请先: bash scripts/fetch-slime-upstream.sh"
  echo "或设置 SLIME_SRC 指向含 slimec（含 --target cnos）的仓库。"
  exit 1
fi

if ! command -v cargo >/dev/null 2>&1; then
  echo "需要 Rust/cargo。参见 https://rustup.rs/"
  exit 1
fi

echo "=== build slimec (Slime for CNOS) ==="
echo "    SLIME_SRC=$SLIME_SRC"
echo "    PREFIX=$PREFIX"
echo "====================================="

mkdir -p "$PREFIX/bin"

( cd "$SLIME_SRC" && cargo build --release )

install -m0755 "$SLIME_SRC/target/release/slimec" "$PREFIX/bin/slimec"

cat > "$PREFIX/bin/cnos-slime-env.sh" << EOF
# Slime for CNOS — source 此文件后再使用 slimec / cnos-slime-compile.sh
export SLIMEC_PREFIX="$PREFIX"
export PATH="$PREFIX/bin:\${PATH}"

# 裸机链接器与 CNAF 头（与 GCC For CNOS 一致）
if [ -f "\${CNOS_APP_ROOT:-}/bin/cnos-app-env.sh" ]; then
  # shellcheck source=/dev/null
  source "\${CNOS_APP_ROOT}/bin/cnos-app-env.sh"
elif [ -n "\${GCC_FOR_CNOS_PREFIX:-}" ] && [ -f "\${GCC_FOR_CNOS_PREFIX}/bin/x86_64-elf-ld" ]; then
  export PATH="\${GCC_FOR_CNOS_PREFIX}/bin:\${PATH}"
fi

# 便于脚本定位 CNOS 源码树（链接 user.ld）
export CNOS_ROOT="${ROOT}"

# Slime CNOS STD（import \"cnos/io\" 等）
export SLIME_PATH="${ROOT}/integrations/slime-for-cnos/std"
EOF
chmod 0755 "$PREFIX/bin/cnos-slime-env.sh"

echo "已安装: $PREFIX/bin/slimec"
echo "环境:   source $PREFIX/bin/cnos-slime-env.sh"
echo "示例:   bash $ROOT/scripts/cnos-slime-compile.sh $ROOT/user/hello.sm /tmp/hello-slime.elf"

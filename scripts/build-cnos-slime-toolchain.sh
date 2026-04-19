#!/usr/bin/env bash
# 构建 slimec（--target cnos）并安装到给定前缀，生成 cnos-slime-env.sh
# ChaserOS 仓库默认源码：integrations/slime-for-chaseros/upstream/slime
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PREFIX="${1:?用法: $0 <安装前缀，如 \$HOME/opt/cnos-slime>}"

SLIME_SRC="${SLIME_SRC:-$ROOT/integrations/slime-for-chaseros/upstream/slime}"

if [ ! -f "$SLIME_SRC/Cargo.toml" ]; then
  echo "未找到 Slime 源码: $SLIME_SRC"
  echo "请先: bash scripts/fetch-slime-upstream.sh"
  echo "或: SLIME_SRC=/path/to/Slime bash $0 $PREFIX"
  exit 1
fi

if ! command -v cargo >/dev/null 2>&1; then
  echo "需要 Rust/cargo。参见 https://rustup.rs/"
  exit 1
fi

echo "=== build slimec (Slime --target cnos) ==="
echo "    SLIME_SRC=$SLIME_SRC"
echo "    PREFIX=$PREFIX"
echo "=========================================="

mkdir -p "$PREFIX/bin"

( cd "$SLIME_SRC" && cargo build --release )

install -m0755 "$SLIME_SRC/target/release/slimec" "$PREFIX/bin/slimec"

cat > "$PREFIX/bin/cnos-slime-env.sh" << EOF
# Slime + ChaserOS（CNOS 目标）— source 后再用 slimec / cnos-slime-compile.sh
export SLIMEC_PREFIX="$PREFIX"
export PATH="$PREFIX/bin:\${PATH}"

export CHASEROS_ROOT="${ROOT}"
export CNOS_ROOT="${ROOT}"

# 裸机链接器（x86_64-elf-ld）与 CNAF 头：与 GCC For CNOS / linux-cnos-app 一致
if [ -f "\${CNOS_APP_ROOT:-}/bin/cnos-app-env.sh" ]; then
  # shellcheck source=/dev/null
  source "\${CNOS_APP_ROOT}/bin/cnos-app-env.sh"
elif [ -n "\${GCC_FOR_CNOS_PREFIX:-}" ] && [ -f "\${GCC_FOR_CNOS_PREFIX}/bin/x86_64-elf-ld" ]; then
  export PATH="\${GCC_FOR_CNOS_PREFIX}/bin:\${PATH}"
fi

# Slime STD（import \"chaseros/io\" 等；与 cnos 目标 ABI 共用）
export SLIME_PATH="${ROOT}/integrations/slime-for-chaseros/std"
EOF
chmod 0755 "$PREFIX/bin/cnos-slime-env.sh"

echo "已安装: $PREFIX/bin/slimec"
echo "环境:   source $PREFIX/bin/cnos-slime-env.sh"
echo "示例:   source $PREFIX/bin/cnos-slime-env.sh && bash $ROOT/scripts/cnos-slime-compile.sh $ROOT/user/hello.sm /tmp/hello-slime.elf"

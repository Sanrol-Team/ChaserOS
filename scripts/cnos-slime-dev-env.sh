#!/usr/bin/env bash
# ChaserOS Slime 用户态 / STD 并行开发 — 在仓库根执行: source scripts/cnos-slime-dev-env.sh
# 设置 CHASEROS_ROOT、SLIME_PATH；若存在 CMake 生成的 slimec 则加入 PATH。
# 注意：本文件设计为 source，不使用 set -e，以免污染调用方 shell。

_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export CHASEROS_ROOT="$(cd "$_SCRIPT_DIR/.." && pwd)"
# 兼容旧变量名，后续建议改用 CHASEROS_ROOT。
export CNOS_ROOT="$CHASEROS_ROOT"
export SLIME_PATH="$CHASEROS_ROOT/integrations/slime-for-chaseros/std"

if [[ -x "$CHASEROS_ROOT/build/slime-tools/slimec" ]]; then
  export PATH="$CHASEROS_ROOT/build/slime-tools:$PATH"
elif [[ -n "${SLIMEC_PREFIX:-}" ]] && [[ -x "${SLIMEC_PREFIX}/bin/slimec" ]]; then
  export PATH="${SLIMEC_PREFIX}/bin:$PATH"
fi

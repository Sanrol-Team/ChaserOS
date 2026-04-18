#!/usr/bin/env bash
# CNOS Slime 用户态 / STD 并行开发 — 在仓库根执行: source scripts/cnos-slime-dev-env.sh
# 设置 CNOS_ROOT、SLIME_PATH；若存在 CMake 生成的 slimec 则加入 PATH。
# 注意：本文件设计为 source，不使用 set -e，以免污染调用方 shell。

_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export CNOS_ROOT="$(cd "$_SCRIPT_DIR/.." && pwd)"
export SLIME_PATH="$CNOS_ROOT/integrations/slime-for-cnos/std"

if [[ -x "$CNOS_ROOT/build/slime-tools/slimec" ]]; then
  export PATH="$CNOS_ROOT/build/slime-tools:$PATH"
elif [[ -n "${SLIMEC_PREFIX:-}" ]] && [[ -x "${SLIMEC_PREFIX}/bin/slimec" ]]; then
  export PATH="${SLIMEC_PREFIX}/bin:$PATH"
fi

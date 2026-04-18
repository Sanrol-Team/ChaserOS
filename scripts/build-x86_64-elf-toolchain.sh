#!/usr/bin/env bash
# 兼容入口：构建 CNOS 默认目标 x86_64-elf，等价于 ANAF_TARGET=x86_64-elf 调用 build-anaf-cross-toolchain.sh
set -euo pipefail
export ANAF_TARGET=x86_64-elf
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/build-anaf-cross-toolchain.sh" "$@"

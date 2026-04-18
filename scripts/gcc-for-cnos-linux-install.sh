#!/usr/bin/env bash
# 「GCC For CNOS」Linux 安装入口（等同 build-cnos-app-linux-toolchain.sh）
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/build-cnos-app-linux-toolchain.sh" "$@"

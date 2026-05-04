#!/usr/bin/env bash
# ChaserOS 新入口：兼容转发到历史脚本名。
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build-cnos-cross-toolchain.sh" "$@"

#!/usr/bin/env bash
# ChaserOS 新入口：兼容转发到历史脚本名。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
source "$SCRIPT_DIR/cnos-slime-dev-env.sh"

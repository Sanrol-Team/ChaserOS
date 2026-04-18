#!/usr/bin/env bash
# 克隆 Slime 上游到 integrations/slime-for-cnos/upstream/slime（用于开发 Slime For CNOS 后端）
# 上游：https://github.com/FORGE24/Slime
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$ROOT/integrations/slime-for-cnos/upstream/slime"
URL="${SLIME_GIT_URL:-https://github.com/FORGE24/Slime.git}"

if [ -d "$DEST/.git" ]; then
  echo "已存在 $DEST ，跳过克隆。若要更新请在该目录内 git pull。"
  exit 0
fi

mkdir -p "$(dirname "$DEST")"
git clone --depth 1 "$URL" "$DEST"
echo "已克隆到: $DEST"
echo "请参阅 integrations/slime-for-cnos/README.txt"

#!/usr/bin/env bash
# Slime → ELF（中间）→ objcopy raw → CNAB → CNAF v0.2（MANIFEST + IMAGE）
# 内核 cnrun 只解析 CNAF 节表与 CNAB 头，不解析 ELF；ELF 仅作链接中间产物。
# 依赖：slimec、nasm、x86_64-elf-ld、x86_64-elf-objcopy（或 CHASEROS_OBJCOPY）、Python3
# 用法（仓库根）：
#   bash scripts/slime-pack-cnaf.sh path/to/demo.sm out.cnaf
# 可选环境变量（与 CMake 一致）：
#   CHASEROS_OBJCOPY   默认 x86_64-elf-objcopy
#   CHASEROS_CNAB_LOAD_BASE   默认 0x400000
#   CHASEROS_CNAB_ENTRY_RVA   默认 0xe0（须与 user.ld 中 _start 一致）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${1:?用法: $0 <input.sm> <out.cnaf>}"
OUT_CNAF="${2:?用法: $0 <input.sm> <out.cnaf>}"
OBJCOPY="${CHASEROS_OBJCOPY:-x86_64-elf-objcopy}"
LOAD_BASE="${CHASEROS_CNAB_LOAD_BASE:-0x400000}"
ENTRY_RVA="${CHASEROS_CNAB_ENTRY_RVA:-0xe0}"

TMP_ELF="$(mktemp "${TMPDIR:-/tmp}/chaseros-slime-cnaf-XXXXXX.elf")"
TMP_RAW="$(mktemp "${TMPDIR:-/tmp}/chaseros-slime-cnaf-XXXXXX.bin")"
TMP_CNAB="$(mktemp "${TMPDIR:-/tmp}/chaseros-slime-cnaf-XXXXXX.cnab")"
cleanup() { rm -f "$TMP_ELF" "$TMP_RAW" "$TMP_CNAB"; }
trap cleanup EXIT

if ! command -v "$OBJCOPY" >/dev/null 2>&1; then
  echo "未找到 objcopy: $OBJCOPY（请安装 GCC For CNOS 或设置 CHASEROS_OBJCOPY）"
  exit 1
fi

bash "$ROOT/scripts/cnos-slime-compile.sh" "$SRC" "$TMP_ELF"
"$OBJCOPY" -O binary "$TMP_ELF" "$TMP_RAW"
python3 "$ROOT/scripts/wrap-cnab.py" --load-base "$LOAD_BASE" --entry-rva "$ENTRY_RVA" \
  "$TMP_RAW" "$TMP_CNAB"
python3 "$ROOT/scripts/pack-cnaf.py" "$OUT_CNAF" "$TMP_CNAB"
echo "OK: $OUT_CNAF"

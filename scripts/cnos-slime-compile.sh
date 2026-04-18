#!/usr/bin/env bash
# Slime → NASM → ELF64（CNOS 用户映像），需已安装 slimec、nasm、x86_64-elf-ld
# 用法: cnos-slime-compile.sh <input.sm> <out.elf>
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SRC="${1:?用法: $0 <input.sm> <out.elf>}"
OUT="${2:?用法: $0 <input.sm> <out.elf>}"
LINK_LD="${CNOS_USER_LD:-$ROOT/user/user.ld}"

if ! command -v slimec >/dev/null 2>&1; then
  echo "未找到 slimec。请先 source CNOS slime 环境，例如:"
  echo "  source \"\\\$HOME/opt/cnos-slime/bin/cnos-slime-env.sh\""
  exit 1
fi
if ! command -v nasm >/dev/null 2>&1; then
  echo "未找到 nasm"
  exit 1
fi
if ! command -v x86_64-elf-ld >/dev/null 2>&1; then
  echo "未找到 x86_64-elf-ld。请安装 GCC For CNOS（见 toolchains/linux-cnos-app/README.txt）并 source cnos-app-env.sh"
  exit 1
fi

TMPDIR="${TMPDIR:-/tmp}"
STEM="$(basename "$SRC" .sm)"
ASM="$TMPDIR/cnos-slime-${STEM}-$$.asm"
OBJ="$TMPDIR/cnos-slime-${STEM}-$$.o"
CNOS_RT_ASM="${CNOS_RT_ASM:-$ROOT/user/cnos-rt/cnos_syscalls.asm}"
CNOS_RT_O="$TMPDIR/cnos-rt-$$.o"

cleanup() { rm -f "$ASM" "$OBJ" "$CNOS_RT_O"; }
trap cleanup EXIT

if [[ -n "${SLIME_PATH:-}" ]]; then
  env "SLIME_PATH=$SLIME_PATH" slimec --target cnos "$SRC" -o "$ASM"
else
  slimec --target cnos "$SRC" -o "$ASM"
fi
nasm -felf64 "$ASM" -o "$OBJ"

if [[ -n "${CNOS_SLIME_LINK_RT:-1}" ]] && [[ -f "$CNOS_RT_ASM" ]]; then
  nasm -felf64 "$CNOS_RT_ASM" -o "$CNOS_RT_O"
  x86_64-elf-ld -nostdlib "-T$LINK_LD" -o "$OUT" "$OBJ" "$CNOS_RT_O"
else
  x86_64-elf-ld -nostdlib "-T$LINK_LD" -o "$OUT" "$OBJ"
fi

echo "OK: $OUT"

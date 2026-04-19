#!/usr/bin/env bash
#
# Slime 源码 → 汇编 → 目标文件 → ar 归档 → CNAFL v0.2（静态库单元）
#
#   bash scripts/cnos-slimec-lib.sh ./lib.sm ./out.cnafl --lib-name mylib
#
# 依赖：slimec（--target cnos）、nasm、GNU ar、python3。
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
  echo "用法: $0 <源码.sm> <输出.cnafl> --lib-name <名称> [--bundle-id ...] [--abi-major N] [--abi-minor M]"
  exit 1
}

[[ "${1:-}" ]] || usage
[[ "${2:-}" ]] || usage
case "${2}" in
  *.cnafl) ;;
  *) echo "第二个参数应为 .cnafl 输出路径"; usage ;;
esac

SRC="$(realpath "$1")"
OUT="$(realpath -m "$2")"
shift 2
mkdir -p "$(dirname "$OUT")"

LIB_NAME=""
BUNDLE_ID="chaseros.slime.generated"
ABI_MAJOR="1"
ABI_MINOR="0"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --lib-name)
      LIB_NAME="${2:?}"
      shift 2
      ;;
    --bundle-id)
      BUNDLE_ID="${2:?}"
      shift 2
      ;;
    --abi-major)
      ABI_MAJOR="${2:?}"
      shift 2
      ;;
    --abi-minor)
      ABI_MINOR="${2:?}"
      shift 2
      ;;
    *)
      usage
      ;;
  esac
done
[[ -n "$LIB_NAME" ]] || usage

ROOT="${CHASEROS_ROOT:-$ROOT}"
if [[ -n "${CHASEROS_APP_ROOT:-}" && -f "${CHASEROS_APP_ROOT}/bin/cnos-app-env.sh" ]]; then
  # shellcheck source=/dev/null
  source "${CHASEROS_APP_ROOT}/bin/cnos-app-env.sh"
fi

SLIMEC_BIN="${CHASEROS_SLIMEC:-}"
if [[ -z "$SLIMEC_BIN" ]] && [[ -x "${ROOT}/build/slime-tools/slimec" ]]; then
  SLIMEC_BIN="${ROOT}/build/slime-tools/slimec"
fi
if [[ -z "$SLIMEC_BIN" ]]; then
  SLIMEC_BIN="$(command -v slimec || true)"
fi
if [[ -z "$SLIMEC_BIN" ]] || [[ ! -x "$SLIMEC_BIN" ]]; then
  echo "找不到 slimec"
  exit 1
fi

NASM_BIN="${CHASEROS_NASM:-$(command -v nasm || true)}"
if [[ -z "$NASM_BIN" ]] || [[ ! -x "$NASM_BIN" ]]; then
  echo "找不到 nasm"
  exit 1
fi

AR_BIN="${CHASEROS_AR:-$(command -v ar || true)}"
if [[ -z "$AR_BIN" ]] || [[ ! -x "$AR_BIN" ]]; then
  echo "找不到 GNU ar"
  exit 1
fi

PYTHON3="${PYTHON3:-$(command -v python3 || true)}"
if [[ -z "$PYTHON3" ]] || [[ ! -x "$PYTHON3" ]]; then
  echo "需要 python3"
  exit 1
fi

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/cnos-slimec-lib.XXXXXX")"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

ASM="$WORKDIR/out.asm"
OBJ="$WORKDIR/out.o"
ARCHIVE="$WORKDIR/lib${LIB_NAME}.a"

# --emit static-lib：生成无 _start 的 NASM，便于 ar 归档（CNAFL IMAGE）
if [[ -n "${SLIME_PATH:-}" ]]; then
  env "SLIME_PATH=$SLIME_PATH" "$SLIMEC_BIN" --target cnos --emit static-lib "$SRC" -o "$ASM"
else
  "$SLIMEC_BIN" --target cnos --emit static-lib "$SRC" -o "$ASM"
fi
"$NASM_BIN" -felf64 "$ASM" -o "$OBJ"
"$AR_BIN" rcs "$ARCHIVE" "$OBJ"

STEM="$(basename "$SRC" .sm)"
"$PYTHON3" "$ROOT/scripts/pack-cnafl.py" "$OUT" "$ARCHIVE" \
  --lib-name "$LIB_NAME" \
  --bundle-id "${BUNDLE_ID}.${STEM}" \
  --name "$STEM" \
  --version "0.1.0" \
  --abi-major "$ABI_MAJOR" \
  --abi-minor "$ABI_MINOR" \
  --lib-kind static

echo "OK: $OUT"

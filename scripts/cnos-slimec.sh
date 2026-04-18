#!/usr/bin/env bash
#
# cnos-slimec — 在宿主机上将 Slime 源码编译为 .cnaf（CNAF v0.2）
#
#   bash scripts/cnos-slimec.sh ./test.sm ./test.cnaf
#
# 依赖：slimec（支持 --target cnos）、nasm、x86_64-elf-ld、python3。
# 说明：**CNOS 内核里的 shell 无法运行 Rust/slimec**；请在开发机终端使用本脚本。
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
  echo "用法: $0 <源码.sm> <输出.cnaf>"
  echo "可选环境变量:"
  echo "  CNOS_SLIMEC     slimec 路径（默认 PATH）"
  echo "  CNOS_ROOT       CNOS 源码根（默认本仓库）"
  echo "  CNOS_APP_ROOT   GCC For CNOS 安装前缀（若已 source cnos-app-env.sh 可省略部分检查）"
  exit 1
}

[[ "${1:-}" ]] || usage
[[ "${2:-}" ]] || usage

SRC="$(realpath "$1")"
OUT="$(realpath -m "$2")"
mkdir -p "$(dirname "$OUT")"
case "$OUT" in
  *.cnaf) ;;
  *)
    echo "第二个参数应为 .cnaf 输出路径"
    usage
    ;;
esac

ROOT="${CNOS_ROOT:-$ROOT}"

if [[ -n "${CNOS_APP_ROOT:-}" && -f "${CNOS_APP_ROOT}/bin/cnos-app-env.sh" ]]; then
  # shellcheck source=/dev/null
  source "${CNOS_APP_ROOT}/bin/cnos-app-env.sh"
fi

SLIMEC_BIN="${CNOS_SLIMEC:-}"
if [[ -z "$SLIMEC_BIN" ]] && [[ -x "${ROOT}/build/slime-tools/slimec" ]]; then
  SLIMEC_BIN="${ROOT}/build/slime-tools/slimec"
fi
if [[ -z "$SLIMEC_BIN" ]]; then
  SLIMEC_BIN="$(command -v slimec || true)"
fi
if [[ -z "$SLIMEC_BIN" ]] || [[ ! -x "$SLIMEC_BIN" ]]; then
  echo "找不到 slimec。请先构建: cmake ... -DCNOS_WITH_SLIME_USER=ON（生成 build/slime-tools/slimec），"
  echo "或 export CNOS_SLIMEC=/path/to/slimec"
  exit 1
fi

LD_BIN="${CNOS_LD:-$(command -v x86_64-elf-ld || true)}"
if [[ -z "$LD_BIN" ]] || [[ ! -x "$LD_BIN" ]]; then
  echo "找不到 x86_64-elf-ld（GCC For CNOS）。请安装工具链或设置 CNOS_LD。"
  exit 1
fi

NASM_BIN="${CNOS_NASM:-$(command -v nasm || true)}"
if [[ -z "$NASM_BIN" ]] || [[ ! -x "$NASM_BIN" ]]; then
  echo "找不到 nasm"
  exit 1
fi

PYTHON3="${PYTHON3:-$(command -v python3 || true)}"
if [[ -z "$PYTHON3" ]] || [[ ! -x "$PYTHON3" ]]; then
  echo "需要 python3 以打包 CNAF"
  exit 1
fi

USER_LD="${CNOS_USER_LD:-$ROOT/user/user.ld}"
if [[ ! -f "$USER_LD" ]]; then
  echo "找不到链接脚本: $USER_LD（可设置 CNOS_USER_LD）"
  exit 1
fi

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/cnos-slimec.XXXXXX")"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

ASM="$WORKDIR/out.asm"
OBJ="$WORKDIR/out.o"
ELF="$WORKDIR/out.elf"
CNOS_RT_ASM="${CNOS_RT_ASM:-$ROOT/user/cnos-rt/cnos_syscalls.asm}"
CNOS_RT_O="$WORKDIR/cnos_rt.o"

if [[ -n "${SLIME_PATH:-}" ]]; then
  env "SLIME_PATH=$SLIME_PATH" "$SLIMEC_BIN" --target cnos "$SRC" -o "$ASM"
else
  "$SLIMEC_BIN" --target cnos "$SRC" -o "$ASM"
fi
"$NASM_BIN" -felf64 "$ASM" -o "$OBJ"

if [[ -n "${CNOS_SLIME_LINK_RT:-1}" ]] && [[ -f "$CNOS_RT_ASM" ]]; then
  "$NASM_BIN" -felf64 "$CNOS_RT_ASM" -o "$CNOS_RT_O"
  "$LD_BIN" -nostdlib "-T$USER_LD" -o "$ELF" "$OBJ" "$CNOS_RT_O"
else
  "$LD_BIN" -nostdlib "-T$USER_LD" -o "$ELF" "$OBJ"
fi

STEM="$(basename "$SRC" .sm)"
"$PYTHON3" "$ROOT/scripts/pack-cnaf.py" "$OUT" "$ELF" \
  --bundle-id "cnos.slime.${STEM}" \
  --name "$STEM" \
  --version "0.1.0"

echo "OK: $OUT"

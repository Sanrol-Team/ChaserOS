#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

export PATH="/home/forge/opt/cross/bin:$PATH"
cmake --build build --target chaseros_iso >/dev/null

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

set +e
timeout 12s qemu-system-x86_64 -cdrom build/chaseros.iso -m 128M -serial stdio -display none >"$LOG" 2>&1
rc=$?
set -e

if [[ $rc -ne 0 && $rc -ne 124 ]]; then
  echo "qemu run failed"
  cat "$LOG"
  exit 1
fi

if ! grep -n "ChaserOS>" "$LOG" >/dev/null; then
  echo "kernel prompt not observed"
  cat "$LOG"
  exit 1
fi

echo "QEMU process smoke passed."

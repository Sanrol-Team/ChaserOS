#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

RAW="$TMP/app.raw"
CNIM="$TMP/app.cnim"
CNPK="$TMP/app.cnpk"
CNLK="$TMP/libdemo.cnlk"

python3 - <<'PY' > "$RAW"
import sys
sys.stdout.buffer.write(b"\x90" * 64)
PY

python3 "$ROOT/scripts/wrap-cnim.py" --load-base 0x400000 --entry-rva 0x0 "$RAW" "$CNIM"
python3 "$ROOT/scripts/pack-cnlk.py" "$CNLK" "$CNIM" --soname demo --abi-major 1 --abi-minor 0 --version 1.0.0
python3 "$ROOT/scripts/pack-cnpk.py" "$CNPK" "$CNIM" --bundle-id demo.app --name Demo --version 1.0.0 --requires demo@1.0

python3 - "$CNPK" "$CNLK" <<'PY'
import struct, sys
cnpk = open(sys.argv[1], "rb").read()
cnlk = open(sys.argv[2], "rb").read()
magic_cnpk = struct.unpack_from("<I", cnpk, 0)[0]
magic_cnlk = struct.unpack_from("<I", cnlk, 0)[0]
assert magic_cnpk == 0x4B504E43, hex(magic_cnpk)
assert magic_cnlk == 0x4B4C4E43, hex(magic_cnlk)
print("cnpk/cnlk header magic check passed")
PY

echo "CNPK smoke test passed."

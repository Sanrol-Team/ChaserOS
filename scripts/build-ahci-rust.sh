#!/usr/bin/env bash
# 仅构建 AHCI Rust staticlib（与 CMake 使用相同 CARGO_TARGET_DIR 布局时可指定 BUILD_DIR）
set -euo pipefail
ROOT="$(cd "$(dirname "${0}")/.." && pwd)"
cd "$ROOT/kernel/ahci_rust"
if ! command -v cargo >/dev/null 2>&1; then
  echo "需要 cargo（与编译 slimec 相同）" >&2
  exit 1
fi
if command -v rustup >/dev/null 2>&1; then
  rustup target add x86_64-unknown-none 2>/dev/null || true
fi
export CARGO_TARGET_DIR="${BUILD_DIR:-$ROOT/build/ahci_rust}"
mkdir -p "$CARGO_TARGET_DIR"
cargo build --release
echo "OK: $CARGO_TARGET_DIR/x86_64-unknown-none/release/libchaseros_ahci_rust.a"

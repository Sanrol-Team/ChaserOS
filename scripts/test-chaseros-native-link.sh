#!/usr/bin/env bash
set -euo pipefail

if [ -z "${1:-}" ]; then
  echo "用法: $0 <native-gcc-prefix>" >&2
  exit 1
fi

PREFIX="$1"
TARGET="${CHASEROS_NATIVE_TARGET:-x86_64-chaseros}"
ENV_SH="$PREFIX/bin/chaseros-native-gcc-env.sh"
if [ ! -f "$ENV_SH" ]; then
  echo "未找到环境脚本: $ENV_SH" >&2
  exit 1
fi

# shellcheck source=/dev/null
source "$ENV_SH"

if ! command -v "$TARGET-gcc" >/dev/null 2>&1; then
  echo "未找到编译器: $TARGET-gcc" >&2
  exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat >"$TMP/min.c" <<'EOF'
#include <chaseros/chaseros_user_runtime.h>
int main(void) {
    static const char msg[] = "hello from x86_64-chaseros-gcc\n";
    chaseros_write(1, msg, sizeof(msg) - 1);
    return 0;
}
EOF

"$TARGET-gcc" $CHASEROS_USER_CFLAGS -c "$TMP/min.c" -o "$TMP/min.o"
"$TARGET-gcc" $CHASEROS_USER_LDFLAGS \
  "$CHASEROS_SYSROOT/usr/lib/crt0.o" "$TMP/min.o" $CHASEROS_USER_LIBS \
  -o "$TMP/min.elf"

if [ ! -s "$TMP/min.elf" ]; then
  echo "链接失败：未生成最小 ELF" >&2
  exit 1
fi

echo "OK: linked $TMP/min.elf"

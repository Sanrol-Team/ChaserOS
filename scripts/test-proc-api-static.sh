#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

check() {
  local pat="$1" file="$2"
  if ! grep -nE "$pat" "$file" >/dev/null; then
    echo "missing pattern '$pat' in $file" >&2
    exit 1
  fi
}

check "CHASEROS_SYS_SPAWN" "$ROOT/kernel/syscall_abi.h"
check "CHASEROS_SYS_EXEC" "$ROOT/kernel/syscall_abi.h"
check "CHASEROS_SYS_WAITPID" "$ROOT/kernel/syscall_abi.h"
check "CHASEROS_SYS__EXIT" "$ROOT/kernel/syscall_abi.h"
check "CHASEROS_SYS_FORK" "$ROOT/kernel/syscall_abi.h"
check "spawn <file>" "$ROOT/kernel/shell.c"
check "wait \\[pid\\]" "$ROOT/kernel/shell.c"
check "user_sys_fork" "$ROOT/kernel/loader/user_exec.c"
check "vmm_try_handle_cow_fault" "$ROOT/kernel/isr.c"

echo "process/exec static API checks passed."

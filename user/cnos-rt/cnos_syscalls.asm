; CNOS 用户态系统调用桩 — 约定见 user/SYSCALL-ABI.txt、kernel/syscall_abi.h
; SysV amd64：参数通过 RDI、RSI、RDX（及预留 R10/R8/R9）传入；供 Slime extern "C" 链接。
;
; CNOS_SYS_WRITE = 1 : EAX=1, RDI=fd, RSI=buf, RDX=len → 返回 RAX（已写字节或 -errno）
; CNOS_SYS_EXIT  = 0 : EAX=0, RDI=exit_code（内核可选用）

global cnos_sys_write
global cnos_sys_exit

section .text

cnos_sys_write:
    mov eax, 1
    int 0x80
    ; RAX 由 ISR 写回（成功字节数 / -errno）
    ret

cnos_sys_exit:
    xor eax, eax
    int 0x80
    ud2

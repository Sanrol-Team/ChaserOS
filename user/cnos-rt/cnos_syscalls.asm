; CNOS 用户态系统调用桩 — 约定见 user/SYSCALL-ABI.txt、kernel/syscall_abi.h
; SysV amd64：参数通过 RDI、RSI、RDX（及预留 R10/R8/R9）传入；供 Slime extern "C" 链接。
;
; CNOS_SYS_WRITE = 1 : EAX=1, RDI=fd, RSI=buf, RDX=len → 返回 RAX
; CNOS_SYS_EXIT  = 0 : EAX=0, RDI=exit_code
; CNOS_SYS_GETPID = 2 : EAX=2 → RAX
; CNOS_SYS_READ   = 3 : EAX=3, RDI=fd, RSI=buf, RDX=len → RAX
; CNOS_SYS_UPTIME_TICKS = 4 : EAX=4 → RAX
; CNOS_SYS_OPEN = 5 : EAX=5, RDI=path, RSI=flags → RAX
; CNOS_SYS_CLOSE = 6 : EAX=6, RDI=fd → RAX
; CNOS_SYS_IPC_SEND = 7 … IPC_REPLY = 9（RDI/RSI 见 ABI）
; CNOS_SYS_IPC_CALL = 10 : EAX=10, RDI=dest, RSI=req, RDX=rep → RAX

global cnos_sys_write
global cnos_sys_exit
global cnos_sys_getpid
global cnos_sys_read
global cnos_sys_uptime_ticks
global cnos_sys_open
global cnos_sys_close
global cnos_sys_ipc_send
global cnos_sys_ipc_recv
global cnos_sys_ipc_reply
global cnos_sys_ipc_call

section .text

cnos_sys_write:
    mov eax, 1
    int 0x80
    ret

cnos_sys_exit:
    xor eax, eax
    int 0x80
    ud2

cnos_sys_getpid:
    mov eax, 2
    int 0x80
    ret

cnos_sys_read:
    mov eax, 3
    int 0x80
    ret

cnos_sys_uptime_ticks:
    mov eax, 4
    int 0x80
    ret

cnos_sys_open:
    mov eax, 5
    int 0x80
    ret

cnos_sys_close:
    mov eax, 6
    int 0x80
    ret

cnos_sys_ipc_send:
    mov eax, 7
    int 0x80
    ret

cnos_sys_ipc_recv:
    mov eax, 8
    int 0x80
    ret

cnos_sys_ipc_reply:
    mov eax, 9
    int 0x80
    ret

cnos_sys_ipc_call:
    mov eax, 10
    int 0x80
    ret

; ---------- message_t 用户缓冲辅助（与 kernel 布局一致：8+8+48） ----------
; RDI = msg*

global cnos_msg_clear
global cnos_msg_set_type
global cnos_msg_ping_init
global cnos_msg_get_type
global cnos_msg_payload_get_i64

cnos_msg_clear:
    push rdi
    mov ecx, 64
    xor eax, eax
    rep stosb
    pop rdi
    ret

cnos_msg_set_type:
    push rdi
    xor eax, eax
    mov qword [rdi], rax
    mov [rdi+8], rsi
    add rdi, 16
    mov ecx, 48
    xor eax, eax
    rep stosb
    pop rdi
    ret

cnos_msg_ping_init:
    mov rsi, 1
    jmp cnos_msg_set_type

cnos_msg_get_type:
    mov rax, [rdi+8]
    ret

cnos_msg_payload_get_i64:
    mov rax, [rdi+16]
    ret

; 可链接的 64+64 字节 scratch，供 Slime 高层演示（单线程占位）
section .bss
align 64
cnos_ipc_buf_req: resb 64
cnos_ipc_buf_rep: resb 64

section .text
global cnos_ipc_buf_req_addr
global cnos_ipc_buf_rep_addr

cnos_ipc_buf_req_addr:
    lea rax, [rel cnos_ipc_buf_req]
    ret

cnos_ipc_buf_rep_addr:
    lea rax, [rel cnos_ipc_buf_rep]
    ret

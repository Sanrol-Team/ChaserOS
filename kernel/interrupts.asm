; kernel/interrupts.asm - 中断处理汇编封装
[BITS 64]

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push byte %1
    jmp isr_common_stub
%endmacro

; 异常处理程序 (0-31)
ISR_NOERRCODE 0  ; 除零错误
ISR_NOERRCODE 1  ; 调试异常
ISR_NOERRCODE 2  ; 不可屏蔽中断
ISR_NOERRCODE 3  ; 断点异常
ISR_NOERRCODE 4  ; 溢出
ISR_NOERRCODE 5  ; 越界
ISR_NOERRCODE 6  ; 无效指令
ISR_NOERRCODE 7  ; 设备不可用
ISR_ERRCODE   8  ; 双重错误
ISR_NOERRCODE 9  ; 协处理器越段
ISR_ERRCODE   10 ; 无效 TSS
ISR_ERRCODE   11 ; 段不存在
ISR_ERRCODE   12 ; 栈段错误
ISR_ERRCODE   13 ; 通用保护错误
ISR_ERRCODE   14 ; 页错误
ISR_NOERRCODE 15 ; 保留
ISR_NOERRCODE 16 ; x87 浮点错误
ISR_ERRCODE   17 ; 对齐检查
ISR_NOERRCODE 18 ; 机器检查
ISR_NOERRCODE 19 ; SIMD 浮点异常
ISR_NOERRCODE 20 ; 虚拟化异常
ISR_NOERRCODE 21 ; 保留
ISR_NOERRCODE 22 ; 保留
ISR_NOERRCODE 23 ; 保留
ISR_NOERRCODE 24 ; 保留
ISR_NOERRCODE 25 ; 保留
ISR_NOERRCODE 26 ; 保留
ISR_NOERRCODE 27 ; 保留
ISR_NOERRCODE 28 ; 保留
ISR_NOERRCODE 29 ; 保留
ISR_NOERRCODE 30 ; 安全异常
ISR_NOERRCODE 31 ; 保留

; 自定义中断 (如系统调用)
ISR_NOERRCODE 128 ; 0x80 系统调用

; IRQ 处理程序 (32-47)
%macro IRQ 2
global irq%1
irq%1:
    cli
    push byte 0
    push byte %2
    jmp isr_common_stub
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern isr_handler

isr_common_stub:
    ; 保存所有通用寄存器 (rax to r15)
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 将当前栈指针作为参数传递给 C 处理程序
    ; 注意：勿把 RSP 写入 proc_t 再在返回时从「新的」current 恢复。IRQ0 里会 schedule()，
    ; current_task_idx 可能已变，若用另一任务的 proc->rsp 恢复会栈错乱并三重故障 / 直接退出。
    mov rdi, rsp
    call isr_handler

    ; 恢复所有通用寄存器
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 清理错误代码和中断号
    add rsp, 16
    
    sti
    iretq

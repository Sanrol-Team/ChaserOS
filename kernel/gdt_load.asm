; kernel/gdt_load.asm - 加载 GDT 与 TSS
[BITS 64]

section .text

global gdt_load
; rdi = struct gdt_ptr * { uint16_t limit; uint64_t base; }
gdt_load:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global load_tss
; TSS 选择子 0x28（GDT 第 5 项）
load_tss:
    mov ax, 0x28
    ltr ax
    ret

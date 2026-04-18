; kernel/entry.asm - Multiboot2 入口，32位到64位长模式
; Multiboot2 头必须出现在首个可加载段开头（与 .text 同段），单独 .multiboot 易导致 GRUB 报 no multiboot header found

SECTION .text
[BITS 32]

extern kernel_main
extern _bss_start
extern _bss_end
global _start

align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    ; checksum：使 magic+arch+length+checksum 及所有 tag 的 u32 之和 ≡ 0 (mod 2^32)
    dd (0x100000000 - (0xE85250D6 + (header_end - header_start))) & 0xffffffff
    ; 勿在此处请求 framebuffer（Multiboot2 type 5）：否则 GRUB 会切图形模式，
    ; 显示器扫描线性帧缓冲，写 0xB8000 文本显存在 QEMU 窗口里不可见。
    align 8
    dw 0, 0
    dd 8
header_end:

_start:
    mov [saved_magic], eax
    mov [saved_mbi], ebx

    mov esp, stack32_top

    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x0002
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9
    mov cr4, eax

    mov eax, pdpt_table
    or eax, 0x03
    mov [pml4_table], eax

    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax

    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax
    add eax, 4096
    mov [pdpt_table + 8], eax
    add eax, 4096
    mov [pdpt_table + 16], eax
    add eax, 4096
    mov [pdpt_table + 24], eax

    mov edi, pd_table
    mov ecx, 2048
    xor ebx, ebx
.map_4gb:
    mov eax, ebx
    or eax, 0x83
    mov [edi], eax
    mov dword [edi + 4], 0
    add edi, 8
    add ebx, 0x200000
    loop .map_4gb

    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_entry

[BITS 64]
long_mode_entry:
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top

    mov eax, [saved_mbi]
    mov rdi, rax

    mov eax, [saved_magic]
    mov rsi, rax

    call kernel_main

.halt:
    hlt
    jmp .halt

SECTION .data
align 4
saved_magic:
    dd 0
saved_mbi:
    dd 0

align 16
gdt64_start:
    dq 0x0
    dq 0x0020980000000000
    dq 0x0000920000000000
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

SECTION .bss
align 16
stack32_bottom:
    resb 4096
stack32_top:

align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096 * 4

align 16
stack_bottom:
    resb 4096
stack_top:

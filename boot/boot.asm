; boot/boot.asm - 简单的16位实模式引导加载程序
; 获取内存地图并加载内核

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET EQU 0x8000 ; 移到 0x8000 以避免覆盖引导程序
MEM_MAP_ADDR EQU 0x1000  ; 内存地图存在低端内存

start:
    ; 显式初始化段寄存器为 0
    jmp 0:init_segs

init_segs:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [BOOT_DRIVE], dl

    ; 打印欢迎信息
    mov si, msg_hello
    call print_string

    ; --- 开启 VBE 图形模式 (1024x768x32) ---
    call setup_vbe

    ; 获取内存地图 (E820)
    call get_memory_map

    ; --- 开启 A20 总线 ---
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 从磁盘加载内核
    mov bx, KERNEL_OFFSET
    mov dh, 64          ; 加载 64 个扇区 (32KB)，确保内核被完整加载
    mov dl, [BOOT_DRIVE]
    call load_disk

    ; 打印加载完成信息
    mov si, msg_loaded
    call print_string

    ; 切换到32位保护模式
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

[BITS 16]
print_string:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    popa
    ret

; 获取 BIOS 内存地图 (E820)
get_memory_map:
    pusha
    mov di, MEM_MAP_ADDR + 4
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150
.loop:
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .error
    test ebx, ebx
    jz .last_entry
    add di, 24
    inc bp
    jmp .loop
.last_entry:
    inc bp
.done:
    mov [MEM_MAP_ADDR], bp
    popa
    ret
.error:
    mov si, msg_mem_err
    call print_string
    jmp $

; --- VBE 图形模式设置 ---
VBE_INFO_BLOCK EQU 0x9000
MODE_INFO_BLOCK EQU 0x9200
VBE_MODE_NUM EQU 0x115   ; 800x600x32
VBE_MODE_LFB EQU 0x4115   ; 800x600x32 + LFB

setup_vbe:
    pusha
    
    ; 获取 VBE 信息
    mov ax, 0x4F00
    mov di, VBE_INFO_BLOCK
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 获取特定模式信息 (800x600x32)
    mov ax, 0x4F01
    mov cx, VBE_MODE_NUM  ; 注意：此处必须用原始模式号
    mov di, MODE_INFO_BLOCK
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 设置 VBE 模式
    mov ax, 0x4F02
    mov bx, VBE_MODE_LFB  ; 注意：此处才用带 LFB 标志的模式号
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    popa
    ret

vbe_error:
    mov si, msg_vbe_err
    call print_string
    ; 如果 VBE 失败，我们依然尝试继续（可能会回退到文本模式，但通常 QEMU 支持 VBE）
    popa
    ret

load_disk:
    pusha
    mov [dh_save], dh   ; 保存要读取的扇区总数
    mov cx, 0x0002      ; ch=0 (柱面), cl=2 (起始扇区)
    mov dh, 0           ; 起始磁头
.read_loop:
    push cx
    push dx
    mov ax, 0x0201      ; ah=02 (读), al=01 (1个扇区)
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    
    pop dx
    pop cx
    
    add bx, 512         ; 移动缓冲区指针
    inc cl              ; 下一个扇区
    cmp cl, 19          ; 每磁道 18 扇区
    jne .next_check
    mov cl, 1
    inc dh              ; 下一个磁头
    cmp dh, 2
    jne .next_check
    mov dh, 0
    inc ch              ; 下一个柱面
.next_check:
    dec byte [dh_save]
    jnz .read_loop
    popa
    ret

dh_save db 0

disk_error:
    mov si, msg_disk_err
    call print_string
    jmp $

[BITS 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000
    mov esp, ebp

    ; 将内存地图地址作为参数传递 (通过 ESI)
    mov esi, MEM_MAP_ADDR
    jmp KERNEL_OFFSET

; GDT
gdt_start:
    dq 0x0
gdt_code:
    dw 0xffff, 0x0
    db 0x0, 10011010b, 11001111b, 0x0
gdt_data:
    dw 0xffff, 0x0
    db 0x0, 10010010b, 11001111b, 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

BOOT_DRIVE db 0
msg_hello db "Booting...", 13, 10, 0
msg_vbe_err db "VBE error!", 13, 10, 0
msg_loaded db "Kernel loaded.", 13, 10, 0
msg_disk_err db "Disk error!", 13, 10, 0
msg_mem_err db "Mem error!", 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55

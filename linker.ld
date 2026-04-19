/* linker.ld - ChaserOS x86_64 混合内核链接脚本 (Multiboot2 / GRUB2) */

ENTRY(_start)

SECTIONS
{
    /*
     * 1MiB 处加载，符合 Multiboot2 / GRUB 常用布局。
     */
    . = 0x100000;

    _kernel_start = .;

    /* Multiboot2 头在 entry.asm 的 .text 最前，勿单独 .multiboot，否则 GRUB 可能找不到头 */
    .text ALIGN(8) :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;
    }

    /*
     * 预留物理页相邻的 BSS 式区域，供日后块设备缓冲 / 文件系统缓存使用。
     * 不计入磁盘上的 ELF 文件体积（NOLOAD），运行时在内存中占位。
     */
    .fs_reserve (NOLOAD) : ALIGN(4096)
    {
        _fs_reserve_start = .;
        . += 0x10000;
        _fs_reserve_end = .;
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }
}

/* user/user.ld - 与内核身份映射一致的低地址可执行文件 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) }
    /DISCARD/ : { *(.eh_frame .comment) }
}

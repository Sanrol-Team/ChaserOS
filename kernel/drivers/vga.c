/* kernel/drivers/vga.c - VGA 文本模式驱动实现 */

#include "vga.h"
#include "../io.h"
#include <stdint.h>

/*
 * 必须用 volatile：否则 -O2 下对 0xB8000 的写可能被优化掉或重排，
 * QEMU VGA 窗口不刷新，而串口 (-serial stdio) 仍正常。
 */
typedef volatile uint16_t vga_cell_t;
static vga_cell_t *const vga_buffer = (vga_cell_t *)VGA_ADDRESS;
static int cursor_x = 0;
static int cursor_y = 0;

/* 更新硬件光标位置 */
void vga_update_cursor() {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init() {
    vga_clear();
}

void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (uint16_t)((0x07 << 8) | ' ');
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            int index = cursor_y * VGA_WIDTH + cursor_x;
            vga_buffer[index] = (uint16_t)((0x07 << 8) | ' ');
        }
    } else {
        int index = cursor_y * VGA_WIDTH + cursor_x;
        vga_buffer[index] = (uint16_t)((0x07 << 8) | c);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        /* 向上滚动一行 */
        for (int y = 0; y < VGA_HEIGHT - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
            }
        }
        /* 清除最后一行 */
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)((0x07 << 8) | ' ');
        }
        cursor_y = VGA_HEIGHT - 1;
    }
    
    vga_update_cursor();
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putchar(*s++);
    }
}

/* kernel/drivers/serial.c - 串口 (UART) 驱动实现 */

#include "serial.h"
#include "../io.h"
#include <stdint.h>

void serial_init() {
    outb(COM1 + 1, 0x00);    // 禁用所有中断
    outb(COM1 + 3, 0x80);    // 启用 DLAB (设置波特率除数)
    outb(COM1 + 0, 0x03);    // 设置除数为 3 (波特率 38400)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 位，无奇偶校验，1 个停止位
    outb(COM1 + 2, 0xC7);    // 启用 FIFO，清除，14 字节阈值
    outb(COM1 + 4, 0x0B);    // IRQ 启用，RTS/DSR 设置
}

static int is_transmit_empty() {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_puts(const char *s) {
    while (*s) {
        serial_putchar(*s++);
    }
}

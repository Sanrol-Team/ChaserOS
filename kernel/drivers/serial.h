/* kernel/drivers/serial.h - 串口 (UART) 驱动 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define COM1 0x3F8

void serial_init();
void serial_putchar(char c);
void serial_puts(const char *s);

#endif

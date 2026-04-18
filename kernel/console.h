/* kernel/console.h - VGA 文本或 Multiboot 帧缓冲 + 字体 */

#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdint.h>

void console_init(uint64_t mbi_phys);
void console_putchar(char c);
void console_puts(const char *s);
void console_clear(void);

int console_is_framebuffer(void);
void console_fb_dims(int *out_w, int *out_h);
uint64_t console_get_mbi_phys(void);
const char *console_font_desc(void);

#endif

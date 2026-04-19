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

/*
 * 帧缓冲 Shell：运行时调整字模大小（1–10，越大字越大）；0＝按屏高自动。
 * 不改变硬件像素模式；真分辨率请改 iso/boot/grub.cfg 中 gfxpayload 后重启。
 */
void console_set_fb_font_scale(int step);
int console_get_fb_font_scale(void);

#endif

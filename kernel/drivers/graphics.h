/* kernel/drivers/graphics.h - 帧缓冲绘图（未编入内核时仅保留头；与 multiboot 共用 vbe_mode_info） */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "../multiboot.h"

void graphics_init(struct vbe_mode_info *vbe);
void graphics_draw_pixel(int x, int y, uint32_t color);
void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
void graphics_clear(uint32_t color);
/* 将帧缓冲内容上移 dy 行（像素），底部用 fill_color 填充 */
void graphics_scroll_up_pixels(int dy, uint32_t fill_color);
void graphics_draw_char(int x, int y, char c, uint32_t color);
/* 整数放大 8x16 位图字体，scale>=1 */
void graphics_draw_char_scaled(int x, int y, char c, uint32_t color, int scale);
void graphics_draw_string(int x, int y, const char *s, uint32_t color);
int graphics_get_width(void);
int graphics_get_height(void);
int graphics_get_pitch(void);
uint32_t *graphics_get_lfb(void);

#endif

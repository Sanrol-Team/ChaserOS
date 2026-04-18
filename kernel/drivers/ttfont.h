/* kernel/drivers/ttfont.h - Shell 字体：缩放位图或可选 stb_truetype */

#ifndef KERNEL_TTFONT_H
#define KERNEL_TTFONT_H

#include <stddef.h>
#include <stdint.h>

void ttfont_reset(void);
void ttfont_init_for_height(int screen_height);

/* 返回 0 表示成功启用矢量字体（需编译时 CNOS_HAVE_STBTT + 模块数据有效） */
int ttfont_init_stb(const void *font_data, size_t font_size);

int ttfont_is_vector(void);
void ttfont_line_metrics(int *line_height_px, int *cell_width_px);

/*
 * 绘制 ASCII 可见字符；矢量模式支持 Latin-1 码点。
 * baseline 左上角约定与 graphics_draw_char 一致（由上向下）。
 */
void ttfont_draw_glyph(int x, int y, unsigned char c, uint32_t fg);

#endif

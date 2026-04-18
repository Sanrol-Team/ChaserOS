/* kernel/drivers/graphics.c - 帧缓冲绘图 */

#include "graphics.h"
#include "font.h"
#include <stddef.h>
#include <stdint.h>

static struct vbe_mode_info *mode_info;
static uintptr_t lfb_base;

void graphics_init(struct vbe_mode_info *vbe) {
    mode_info = vbe;
    lfb_base = (uintptr_t)vbe->framebuffer;
}

void graphics_draw_pixel(int x, int y, uint32_t color) {
    if (!mode_info || lfb_base == 0) {
        return;
    }
    if (x < 0 || x >= mode_info->width || y < 0 || y >= mode_info->height) {
        return;
    }

    uint8_t *pixel_ptr = (uint8_t *)lfb_base + (y * (size_t)mode_info->pitch) + (x * 4);
    *(uint32_t *)pixel_ptr = color;
}

void graphics_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            graphics_draw_pixel(x + i, y + j, color);
        }
    }
}

void graphics_clear(uint32_t color) {
    if (!mode_info || lfb_base == 0) {
        return;
    }
    graphics_draw_rect(0, 0, mode_info->width, mode_info->height, color);
}

void graphics_scroll_up_pixels(int dy, uint32_t fill_color) {
    if (!mode_info || lfb_base == 0 || dy <= 0) {
        return;
    }
    int h = mode_info->height;
    int pitch = mode_info->pitch;
    int w = mode_info->width;
    if (dy >= h) {
        graphics_clear(fill_color);
        return;
    }
    uint8_t *base = (uint8_t *)lfb_base;
    for (int row = 0; row < h - dy; row++) {
        uint8_t *dst = base + (size_t)row * (size_t)pitch;
        uint8_t *src = base + (size_t)(row + dy) * (size_t)pitch;
        for (int i = 0; i < pitch; i++) {
            dst[i] = src[i];
        }
    }
    graphics_draw_rect(0, h - dy, w, dy, fill_color);
}

void graphics_draw_char(int x, int y, char c, uint32_t color) {
    graphics_draw_char_scaled(x, y, c, color, 1);
}

void graphics_draw_char_scaled(int x, int y, char c, uint32_t color, int scale) {
    if (!mode_info || lfb_base == 0 || scale < 1) {
        return;
    }
    if ((unsigned char)c > 127) {
        return;
    }

    unsigned char *bitmap = font8x16[(unsigned char)c];
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 8; i++) {
            if (bitmap[j] & (1 << (7 - i))) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        graphics_draw_pixel(x + i * scale + sx, y + j * scale + sy, color);
                    }
                }
            }
        }
    }
}

void graphics_draw_string(int x, int y, const char *s, uint32_t color) {
    int cur_x = x;
    while (*s) {
        if (*s == '\n') {
            cur_x = x;
            y += 16;
        } else {
            graphics_draw_char(cur_x, y, *s, color);
            cur_x += 8;
        }
        s++;
    }
}

int graphics_get_width(void) {
    return mode_info ? mode_info->width : 0;
}

int graphics_get_height(void) {
    return mode_info ? mode_info->height : 0;
}

int graphics_get_pitch(void) {
    return mode_info ? mode_info->pitch : 0;
}

uint32_t *graphics_get_lfb(void) {
    return (uint32_t *)(void *)lfb_base;
}

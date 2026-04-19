/* kernel/console.c - Multiboot 帧缓冲 Shell 或 VGA 文本 */

#include "console.h"
#include "multiboot.h"
#include "drivers/graphics.h"
#include "drivers/ttfont.h"
#include "drivers/vga.h"
#include <stddef.h>
#include <stdint.h>

static uint64_t g_mbi_phys;
static int s_fb_mode;
static int s_vga_text;
static struct vbe_mode_info s_vbe;
static uint32_t s_fg = 0xFFE0E0E0u;
static uint32_t s_bg = 0xFF101010u;

static int pen_x;
static int pen_y;

static int s_fb_phys_h;
static int s_fb_font_manual;

static void layout_metrics(int *lh, int *cw) {
    ttfont_line_metrics(lh, cw);
}

void console_init(uint64_t mbi_phys) {
    g_mbi_phys = mbi_phys;
    s_fb_mode = 0;
    s_vga_text = 0;

    struct vbe_mode_info vbe;
    if (multiboot_fill_vbe(mbi_phys, &vbe) == 0 && vbe.framebuffer != 0 && vbe.width > 0 && vbe.height > 0 &&
        vbe.bpp >= 24) {
        s_vbe = vbe;
        s_fb_phys_h = (int)vbe.height;
        s_fb_font_manual = 0;
        graphics_init(&s_vbe);
        s_fb_mode = 1;
        ttfont_reset();
        ttfont_init_for_height(s_fb_phys_h);
        uint32_t ms = 0, me = 0;
        int ok = -1;
        if (multiboot_find_module_suffix(mbi_phys, "font.ttf", &ms, &me) == 0 && me > ms) {
            ok = ttfont_init_stb((const void *)(uintptr_t)ms, (size_t)(me - ms));
        }
        if (ok != 0 && multiboot_find_module_suffix(mbi_phys, "font.otf", &ms, &me) == 0 && me > ms) {
            ok = ttfont_init_stb((const void *)(uintptr_t)ms, (size_t)(me - ms));
        }
        if (ok == 0) {
            ttfont_init_for_height(s_fb_phys_h);
        }
        graphics_clear(s_bg);
        pen_x = 8;
        pen_y = 8;
        return;
    }

    ttfont_reset();
    /*
     * UEFI/GOP 下固件仍在扫描线性帧缓冲时，写 VGA 文本 0xB8000 会在窗口上变成乱块花屏。
     * 若 MBI 含 EFI 标签且无可用 RGB 帧缓冲，则不要回退到 VGA，仅依赖串口。
     */
    if (multiboot_has_efi_handoff(mbi_phys)) {
        return;
    }
    vga_init();
    s_vga_text = 1;
}

int console_is_framebuffer(void) {
    return s_fb_mode;
}

void console_fb_dims(int *out_w, int *out_h) {
    if (!s_fb_mode) {
        if (out_w) {
            *out_w = 0;
        }
        if (out_h) {
            *out_h = 0;
        }
        return;
    }
    if (out_w) {
        *out_w = graphics_get_width();
    }
    if (out_h) {
        *out_h = graphics_get_height();
    }
}

uint64_t console_get_mbi_phys(void) {
    return g_mbi_phys;
}

static char s_font_desc_buf[64];

const char *console_font_desc(void) {
    int lh = 16, cw = 8;
    ttfont_line_metrics(&lh, &cw);
    if (ttfont_is_vector()) {
        /* 简单描述 */
        char *p = s_font_desc_buf;
        const char *pre = "TrueType vector (module font.ttf), line ";
        int i = 0;
        while (pre[i] && i < (int)sizeof(s_font_desc_buf) - 24) {
            p[i] = pre[i];
            i++;
        }
        p[i] = '\0';
        /* 追加数字简化：略去 puts_dec 依赖，用手写 */
        int n = lh;
        char tmp[16];
        int ti = 0;
        if (n == 0) {
            tmp[ti++] = '0';
        } else {
            while (n > 0 && ti < 15) {
                tmp[ti++] = (char)('0' + (n % 10));
                n /= 10;
            }
        }
        while (ti > 0) {
            p[i++] = tmp[--ti];
        }
        p[i] = '\0';
        return s_font_desc_buf;
    }
    /* 位图缩放 */
    int scale = cw / 8;
    if (scale < 1) {
        scale = 1;
    }
    char *p = s_font_desc_buf;
    const char *a = "Bitmap 8x16 scaled x";
    int j = 0;
    while (a[j] && j < 40) {
        p[j] = a[j];
        j++;
    }
    if (scale >= 10) {
        p[j++] = (char)('0' + scale / 10);
    }
    p[j++] = (char)('0' + scale % 10);
    p[j] = '\0';
    return s_font_desc_buf;
}

void console_clear(void) {
    if (s_fb_mode) {
        graphics_clear(s_bg);
        pen_x = 8;
        pen_y = 8;
        return;
    }
    if (!s_vga_text) {
        return;
    }
    vga_clear();
}

void console_putchar(char c) {
    if (!s_fb_mode) {
        if (!s_vga_text) {
            return;
        }
        vga_putchar(c);
        return;
    }

    int lh, cw;
    layout_metrics(&lh, &cw);
    int max_x = graphics_get_width() - 8;
    int max_y = graphics_get_height() - 8;

    if (c == '\n') {
        pen_x = 8;
        pen_y += lh;
        if (pen_y + lh > max_y) {
            graphics_scroll_up_pixels(lh, s_bg);
            pen_y -= lh;
            if (pen_y < 8) {
                pen_y = 8;
            }
        }
        return;
    }

    if (c == '\b') {
        if (pen_x > 8) {
            pen_x -= cw;
            graphics_draw_rect(pen_x, pen_y, cw, lh, s_bg);
        }
        return;
    }

    if ((unsigned char)c < 32 || (unsigned char)c >= 127) {
        return;
    }

    if (pen_x + cw > max_x) {
        pen_x = 8;
        pen_y += lh;
        if (pen_y + lh > max_y) {
            graphics_scroll_up_pixels(lh, s_bg);
            pen_y -= lh;
            if (pen_y < 8) {
                pen_y = 8;
            }
        }
    }

    ttfont_draw_glyph(pen_x, pen_y, (unsigned char)c, s_fg);
    pen_x += cw;
}

void console_puts(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        console_putchar(*s++);
    }
}

void console_set_fb_font_scale(int step) {
    if (!s_fb_mode) {
        return;
    }
    if (step < 0 || step > 10) {
        return;
    }
    s_fb_font_manual = step;
    ttfont_init_for_height(s_fb_phys_h);
    if (step >= 1 && step <= 10 && !ttfont_is_vector()) {
        ttfont_set_bitmap_scale(step);
    }
    graphics_clear(s_bg);
    pen_x = 8;
    pen_y = 8;
}

int console_get_fb_font_scale(void) {
    if (!s_fb_mode) {
        return 0;
    }
    if (s_fb_font_manual >= 1 && s_fb_font_manual <= 10) {
        return s_fb_font_manual;
    }
    return ttfont_get_bitmap_scale();
}

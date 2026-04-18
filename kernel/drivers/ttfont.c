/* kernel/drivers/ttfont.c - 缩放 VGA 字体；可选 stb_truetype（CNOS_HAVE_STBTT） */

#include "ttfont.h"
#include "graphics.h"
#include <stddef.h>
#include <stdint.h>

static int s_scale_bm = 1;

#ifdef CNOS_HAVE_STBTT
#define STBTT_malloc(x, u) tt_alloc(x, u)
#define STBTT_free(x, u)   tt_free(x, u)

static unsigned char tt_pool[3u * 1024u * 1024u];
static size_t tt_used;

static void *tt_alloc(size_t n, void *u) {
    (void)u;
    n = (n + 15u) & ~15u;
    if (tt_used + n > sizeof(tt_pool)) {
        return NULL;
    }
    void *p = tt_pool + tt_used;
    tt_used += n;
    return p;
}

static void tt_free(void *p, void *u) {
    (void)p;
    (void)u;
}

#include "stb_truetype.h"

static stbtt_fontinfo g_font;
static float g_scale = 12.0f;
static int g_vec_ok;
static int g_line_px;
static int g_cell_px;
#endif

void ttfont_reset(void) {
    s_scale_bm = 1;
#ifdef CNOS_HAVE_STBTT
    tt_used = 0;
    g_vec_ok = 0;
#endif
}

void ttfont_init_for_height(int screen_height) {
    ttfont_reset();
    int s = screen_height / 36;
    if (s < 1) {
        s = 1;
    }
    if (s > 10) {
        s = 10;
    }
    s_scale_bm = s;
#ifdef CNOS_HAVE_STBTT
    if (g_vec_ok) {
        g_scale = (float)(screen_height / 42);
        if (g_scale < 8.0f) {
            g_scale = 8.0f;
        }
        if (g_scale > 48.0f) {
            g_scale = 48.0f;
        }
        int ascent, descent, ld;
        stbtt_GetFontVMetrics(&g_font, &ascent, &descent, &ld);
        g_line_px = (int)(g_scale * (float)(ascent - descent + ld) + 1.0f);
        int adv;
        stbtt_GetCodepointHMetrics(&g_font, 'M', &adv, NULL);
        g_cell_px = (int)(g_scale * (float)adv + 0.5f);
        if (g_cell_px < 4) {
            g_cell_px = 4;
        }
        if (g_line_px < 8) {
            g_line_px = 8;
        }
    }
#endif
}

int ttfont_init_stb(const void *font_data, size_t font_size) {
#ifdef CNOS_HAVE_STBTT
    if (!font_data || font_size < 12u) {
        return -1;
    }
    tt_used = 0;
    if (!stbtt_InitFont(&g_font, (const unsigned char *)font_data, 0)) {
        g_vec_ok = 0;
        return -1;
    }
    g_vec_ok = 1;
    return 0;
#else
    (void)font_data;
    (void)font_size;
    return -1;
#endif
}

int ttfont_is_vector(void) {
#ifdef CNOS_HAVE_STBTT
    return g_vec_ok;
#else
    return 0;
#endif
}

void ttfont_line_metrics(int *line_height_px, int *cell_width_px) {
#ifdef CNOS_HAVE_STBTT
    if (g_vec_ok) {
        *line_height_px = g_line_px;
        *cell_width_px = g_cell_px;
        return;
    }
#endif
    *line_height_px = 16 * s_scale_bm;
    *cell_width_px = 8 * s_scale_bm;
}

void ttfont_draw_glyph(int x, int y, unsigned char c, uint32_t fg) {
#ifdef CNOS_HAVE_STBTT
    if (g_vec_ok) {
        int w, h, xoff, yoff;
        unsigned char *bm =
            stbtt_GetCodepointBitmap(&g_font, g_scale, g_scale, (int)c, &w, &h, &xoff, &yoff);
        if (!bm) {
            return;
        }
        int px = x + xoff;
        int py = y + yoff;
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                unsigned char a = bm[j * w + i];
                if (a > 40) {
                    graphics_draw_pixel(px + i, py + j, fg);
                }
            }
        }
        stbtt_FreeBitmap(bm, NULL);
        return;
    }
#endif
    if (c < 128u) {
        graphics_draw_char_scaled(x, y, (char)c, fg, s_scale_bm);
    }
}

/* kernel/drivers/gui.c - 类 Windows GUI 界面实现 */

#include "gui.h"
#include "graphics.h"
#include "sheet.h"
#include <stddef.h>

static struct SHTCTL *shtctl;
static struct SHEET *sht_back, *sht_win, *sht_mouse;
static uint32_t *buf_back, *buf_win, *buf_mouse;

/* 鼠标指针图形 (16x16) */
static const char mouse_cursor_map[16][16] = {
    "**************..",
    "*OOOOOOOOOOO*...",
    "*OOOOOOOOOO*....",
    "*OOOOOOOOO*.....",
    "*OOOOOOOO*......",
    "*OOOOOOO*.......",
    "*OOOOOOO*.......",
    "*OOOOOOOO*......",
    "*OOOO**OOO*.....",
    "*OOO*..*OOO*....",
    "*OO*....*OOO*...",
    "*O*......*OOO*..",
    "**........*OOO*.",
    "..........*OOO*.",
    "...........***..",
    "................"
};

void gui_handle_mouse(int dx, int dy, int btn) {
    if (!sht_mouse) return;
    
    int mx = sht_mouse->vx0 + dx;
    int my = sht_mouse->vy0 + dy;

    if (mx < 0) mx = 0;
    if (mx > shtctl->xsize - 1) mx = shtctl->xsize - 1;
    if (my < 0) my = 0;
    if (my > shtctl->ysize - 1) my = shtctl->ysize - 1;

    sheet_slide(sht_mouse, mx, my);
    (void)btn;
}

/* 在缓冲区中绘制实心矩形 */
void draw_rect_buf(uint32_t *buf, int bxsize, int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            buf[(y + j) * bxsize + (x + i)] = color;
        }
    }
}

/* 在缓冲区中绘制字符 (使用 8x16 字体) */
void draw_char_buf(uint32_t *buf, int bxsize, int x, int y, char c, uint32_t color) {
    extern unsigned char font8x16[128][16];
    if ((unsigned char)c > 127) return;
    unsigned char *bitmap = font8x16[(unsigned char)c];
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 8; i++) {
            if (bitmap[j] & (1 << (7 - i))) {
                buf[(y + j) * bxsize + (x + i)] = color;
            }
        }
    }
}

/* 在缓冲区中绘制字符串 */
void draw_string_buf(uint32_t *buf, int bxsize, int x, int y, const char *s, uint32_t color) {
    while (*s) {
        draw_char_buf(buf, bxsize, x, y, *s, color);
        x += 8;
        s++;
    }
}

void gui_draw_button(uint32_t *buf, int bxsize, int x, int y, int w, int h, const char *text) {
    /* 绘制主体背景 */
    draw_rect_buf(buf, bxsize, x, y, w, h, COLOR_BTN_FACE);
    
    /* 绘制 3D 边框 */
    for (int i = 0; i < w; i++) {
        buf[y * bxsize + (x + i)] = COLOR_WHITE;
        buf[(y + h - 1) * bxsize + (x + i)] = COLOR_BLACK;
    }
    for (int j = 0; j < h; j++) {
        buf[(y + j) * bxsize + x] = COLOR_WHITE;
        buf[(y + j) * bxsize + (x + w - 1)] = COLOR_BLACK;
    }
    
    /* 居中绘制文本 (简单估算) */
    int len = 0;
    for (const char *p = text; *p; p++) len++;
    draw_string_buf(buf, bxsize, x + (w - len * 8) / 2, y + (h - 16) / 2, text, COLOR_BLACK);
}

void gui_draw_window(uint32_t *buf, int bxsize, int w, int h, const char *title) {
    /* 窗体背景 */
    draw_rect_buf(buf, bxsize, 0, 0, w, h, COLOR_BTN_FACE);
    /* 3D 边框 */
    for (int i = 0; i < w; i++) {
        buf[0 * bxsize + i] = COLOR_WHITE;
        buf[1 * bxsize + i] = COLOR_WHITE;
        buf[(h - 1) * bxsize + i] = COLOR_BLACK;
        buf[(h - 2) * bxsize + i] = COLOR_DARK_GRAY;
    }
    for (int j = 0; j < h; j++) {
        buf[j * bxsize + 0] = COLOR_WHITE;
        buf[j * bxsize + 1] = COLOR_WHITE;
        buf[j * bxsize + (w - 1)] = COLOR_BLACK;
        buf[j * bxsize + (w - 2)] = COLOR_DARK_GRAY;
    }
    /* 标题栏 */
    draw_rect_buf(buf, bxsize, 3, 3, w - 6, 18, COLOR_WINDOW_TITLE);
    draw_string_buf(buf, bxsize, 10, 4, title, COLOR_WHITE);
    
    /* 关闭按钮 */
    gui_draw_button(buf, bxsize, w - 21, 5, 16, 14, "X");
}

extern uint32_t *graphics_get_lfb();
extern int graphics_get_pitch();

void gui_init() {
    int screen_w = graphics_get_width();
    int screen_h = graphics_get_height();
    int pitch = graphics_get_pitch();

    /* 初始化图层控制器 */
    shtctl = shtctl_init(graphics_get_lfb(), screen_w, screen_h, pitch);

    /* 1. 背景图层 */
    sht_back = sheet_alloc(shtctl);
    buf_back = (uint32_t *)0x2000000;
    sheet_setbuf(sht_back, buf_back, screen_w, screen_h, -1);
    
    /* 填充桌面背景 */
    draw_rect_buf(buf_back, screen_w, 0, 0, screen_w, screen_h - 28, COLOR_DESKTOP);
    
    /* 任务栏 (底部) */
    draw_rect_buf(buf_back, screen_w, 0, screen_h - 28, screen_w, 28, COLOR_TASKBAR);
    /* 任务栏上边框 (白色 3D 效果) */
    for (int i = 0; i < screen_w; i++) buf_back[(screen_h - 28) * screen_w + i] = COLOR_WHITE;
    
    /* 开始按钮 */
    gui_draw_button(buf_back, screen_w, 2, screen_h - 26, 60, 24, "Start");

    /* 2. 窗口图层 */
    sht_win = sheet_alloc(shtctl);
    buf_win = (uint32_t *)0x2400000;
    sheet_setbuf(sht_win, buf_win, 400, 300, -1);
    gui_draw_window(buf_win, 400, 400, 300, "System Console");
    draw_string_buf(buf_win, 400, 10, 30, "Welcome to CNOS 64-bit!", COLOR_BLACK);
    draw_string_buf(buf_win, 400, 10, 50, "Microkernel architecture", COLOR_BLACK);
    draw_string_buf(buf_win, 400, 10, 70, "Windows-like UI", COLOR_BLACK);

    /* 3. 鼠标图层 */
    sht_mouse = sheet_alloc(shtctl);
    buf_mouse = (uint32_t *)0x2600000;
    sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); /* 99 为透明 */
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (mouse_cursor_map[y][x] == '*') buf_mouse[y * 16 + x] = COLOR_BLACK;
            else if (mouse_cursor_map[y][x] == 'O') buf_mouse[y * 16 + x] = COLOR_WHITE;
            else buf_mouse[y * 16 + x] = 99;
        }
    }

    sheet_slide(sht_back, 0, 0);
    sheet_slide(sht_win, (screen_w - 400) / 2, (screen_h - 300) / 2);
    sheet_slide(sht_mouse, screen_w / 2, screen_h / 2);
    
    sheet_updown(sht_back, 0);
    sheet_updown(sht_win, 1);
    sheet_updown(sht_mouse, 2);
}

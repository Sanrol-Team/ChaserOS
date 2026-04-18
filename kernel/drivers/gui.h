/* kernel/drivers/gui.h - 类 Windows GUI 界面设计 */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>

/* 颜色定义 (经典 Windows 风格) */
#define COLOR_DESKTOP    0x008080 /* 经典青绿色背景 */
#define COLOR_TASKBAR    0xC0C0C0 /* 银灰色任务栏 */
#define COLOR_BTN_FACE   0xD4D0C8
#define COLOR_BTN_HIGHLIGHT 0xFFFFFF
#define COLOR_BTN_SHADOW 0x808080
#define COLOR_WINDOW_TITLE 0x000080 /* 蓝色标题栏 */
#define COLOR_WHITE      0xFFFFFF
#define COLOR_BLACK      0x000000
#define COLOR_DARK_GRAY  0x808080

/* 初始化 GUI 桌面 */
void gui_init();

/* 绘制一个 Windows 风格的按钮 */
void gui_draw_button(uint32_t *buf, int bxsize, int x, int y, int w, int h, const char *text);

/* 绘制一个带标题栏的窗口 (填充整个缓冲区) */
void gui_draw_window(uint32_t *buf, int bxsize, int w, int h, const char *title);

/* 鼠标交互处理 */
void gui_handle_mouse(int dx, int dy, int btn);

#endif

/* kernel/drivers/mouse.h - PS/2 鼠标驱动 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    uint8_t buf[3];
    uint8_t phase;
    int x, y, btn;
} mouse_dec_t;

/* 初始化并启用鼠标 */
void mouse_init();

/* 处理鼠标原始数据 */
int mouse_decode(mouse_dec_t *mdec, uint8_t dat);

/* 鼠标中断处理程序入口 (由 isr.c 调用) */
void mouse_handler();

#endif

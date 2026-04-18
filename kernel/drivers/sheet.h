/* kernel/drivers/sheet.h - 图层 (Sheet) 管理系统 */

#ifndef SHEET_H
#define SHEET_H

#include <stdint.h>

#define MAX_SHEETS 256

struct SHEET {
    uint32_t *buf;      /* 图层像素缓冲区 */
    int bxsize, bysize; /* 图层自身的尺寸 */
    int vx0, vy0;       /* 图层在屏幕上的坐标 */
    int col_inv;        /* 透明色颜色值 (-1 表示无透明色) */
    int height;         /* 图层高度 (Z-轴顺序) */
    int flags;          /* 状态标志 (是否在使用等) */
    struct SHTCTL *ctl; /* 所属的控制器 */
};

struct SHTCTL {
    uint32_t *vram;     /* 显存地址 (LFB) */
    uint8_t *map;       /* 图层映射表，记录每个像素属于哪个图层 ID */
    int xsize, ysize;   /* 屏幕分辨率 */
    int pitch;          /* 屏幕每行字节数 (VBE pitch) */
    int top;            /* 当前最顶层的高度 */
    struct SHEET *sheets[MAX_SHEETS];   /* 按高度排序的图层指针 */
    struct SHEET sheets0[MAX_SHEETS];  /* 所有的图层对象池 */
};

/* 初始化图层控制器 */
struct SHTCTL *shtctl_init(uint32_t *vram, int xsize, int ysize, int pitch);

/* 分配一个新图层 */
struct SHEET *sheet_alloc(struct SHTCTL *ctl);

/* 设置图层缓冲区 */
void sheet_setbuf(struct SHEET *sht, uint32_t *buf, int xsize, int ysize, int col_inv);

/* 改变图层高度 (移动 Z-轴) */
void sheet_updown(struct SHEET *sht, int height);

/* 刷新图层特定区域 */
void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1);

/* 移动图层坐标 */
void sheet_slide(struct SHEET *sht, int vx0, int vy0);

/* 释放图层 */
void sheet_free(struct SHEET *sht);

#endif

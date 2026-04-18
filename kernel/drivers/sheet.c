/* kernel/drivers/sheet.c - 图层 (Sheet) 管理系统实现 */

#include "sheet.h"
#include "../pmm.h"
#include <stddef.h>

#define SHEET_USE 1

struct SHTCTL *shtctl_init(uint32_t *vram, int xsize, int ysize, int pitch) {
    static struct SHTCTL ctl_instance;
    struct SHTCTL *ctl = &ctl_instance;
    
    ctl->vram = vram;
    ctl->xsize = xsize;
    ctl->ysize = ysize;
    ctl->pitch = pitch;
    ctl->top = -1; /* 初始没有图层 */
    
    /* 分配图层映射表 (Map) */
    /* 800x600 = 480000 字节，约 118 页 */
    /* 我们暂时硬编码一个地址，以后应该用内存管理器分配 */
    ctl->map = (uint8_t *)0x3000000; 
    
    for (int i = 0; i < MAX_SHEETS; i++) {
        ctl->sheets0[i].flags = 0; /* 未使用 */
        ctl->sheets0[i].ctl = ctl;
    }
    
    return ctl;
}

struct SHEET *sheet_alloc(struct SHTCTL *ctl) {
    for (int i = 0; i < MAX_SHEETS; i++) {
        if (ctl->sheets0[i].flags == 0) {
            struct SHEET *sht = &ctl->sheets0[i];
            sht->flags = SHEET_USE;
            sht->height = -1; /* 默认隐藏 */
            return sht;
        }
    }
    return NULL;
}

void sheet_setbuf(struct SHEET *sht, uint32_t *buf, int xsize, int ysize, int col_inv) {
    sht->buf = buf;
    sht->bxsize = xsize;
    sht->bysize = ysize;
    sht->col_inv = col_inv;
}

/* 刷新图层映射表：记录每个像素属于哪个图层 ID */
void sheet_refreshmap(struct SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0) {
    int h, bx, by, vx, vy, bx0, by0, bx1, by1;
    uint8_t *map = ctl->map, sid;
    uint32_t *buf;
    struct SHEET *sht;

    if (vx0 < 0) vx0 = 0;
    if (vy0 < 0) vy0 = 0;
    if (vx1 > ctl->xsize) vx1 = ctl->xsize;
    if (vy1 > ctl->ysize) vy1 = ctl->ysize;

    for (h = h0; h <= ctl->top; h++) {
        sht = ctl->sheets[h];
        sid = (uint8_t)(sht - ctl->sheets0); /* 使用图层在池中的索引作为 ID */
        buf = sht->buf;

        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;

        if (bx0 < 0) bx0 = 0;
        if (by0 < 0) by0 = 0;
        if (bx1 > sht->bxsize) bx1 = sht->bxsize;
        if (by1 > sht->bysize) by1 = sht->bysize;

        for (by = by0; by < by1; by++) {
            vy = sht->vy0 + by;
            for (bx = bx0; bx < bx1; bx++) {
                vx = sht->vx0 + bx;
                if (sht->col_inv == -1 || buf[by * sht->bxsize + bx] != (uint32_t)sht->col_inv) {
                    map[vy * ctl->xsize + vx] = sid;
                }
            }
        }
    }
}

/* 内部刷新函数：将指定高度范围内的图层刷新到显存 */
static void sheet_refreshsub(struct SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1) {
    int h, bx, by, vx, vy, bx0, by0, bx1, by1;
    uint32_t *buf, color;
    uint8_t *map = ctl->map, sid;
    struct SHEET *sht;

    /* 范围限制 */
    if (vx0 < 0) vx0 = 0;
    if (vy0 < 0) vy0 = 0;
    if (vx1 > ctl->xsize) vx1 = ctl->xsize;
    if (vy1 > ctl->ysize) vy1 = ctl->ysize;

    for (h = h0; h <= h1; h++) {
        sht = ctl->sheets[h];
        buf = sht->buf;
        sid = (uint8_t)(sht - ctl->sheets0);
        
        /* 计算该图层需要刷新的局部坐标 */
        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;

        if (bx0 < 0) bx0 = 0;
        if (by0 < 0) by0 = 0;
        if (bx1 > sht->bxsize) bx1 = sht->bxsize;
        if (by1 > sht->bysize) by1 = sht->bysize;

        for (by = by0; by < by1; by++) {
            vy = sht->vy0 + by;
            for (bx = bx0; bx < bx1; bx++) {
                vx = sht->vx0 + bx;
                /* 关键改进：只有当 map 中的 ID 匹配当前图层时才写入显存 */
                if (map[vy * ctl->xsize + vx] == sid) {
                    color = buf[by * sht->bxsize + bx];
                    /* 显存寻址使用 pitch */
                    uint32_t *vram_ptr = (uint32_t *)((uint8_t *)ctl->vram + vy * ctl->pitch + vx * 4);
                    *vram_ptr = color;
                }
            }
        }
    }
}

void sheet_updown(struct SHEET *sht, int height) {
    struct SHTCTL *ctl = sht->ctl;
    int h, old = sht->height;

    if (height > ctl->top + 1) height = ctl->top + 1;
    if (height < -1) height = -1;
    sht->height = height;

    if (old > height) { /* 降低高度 */
        if (height >= 0) {
            for (h = old; h > height; h--) {
                ctl->sheets[h] = ctl->sheets[h - 1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
            sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1, old);
        } else { /* 隐藏 */
            if (ctl->top > old) {
                for (h = old; h < ctl->top; h++) {
                    ctl->sheets[h] = ctl->sheets[h + 1];
                    ctl->sheets[h]->height = h;
                }
            }
            ctl->top--;
            sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0, ctl->top);
        }
    } else if (old < height) { /* 提高高度 */
        if (old >= 0) {
            for (h = old; h < height; h++) {
                ctl->sheets[h] = ctl->sheets[h + 1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
        } else { /* 显示隐藏的图层 */
            for (h = ctl->top; h >= height; h--) {
                ctl->sheets[h + 1] = ctl->sheets[h];
                ctl->sheets[h + 1]->height = h + 1;
            }
            ctl->sheets[height] = sht;
            ctl->top++;
        }
        sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height);
        sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height, height);
    }
}

void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1) {
    if (sht->height >= 0) {
        sheet_refreshsub(sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height, sht->height);
    }
}

void sheet_slide(struct SHEET *sht, int vx0, int vy0) {
    int old_vx0 = sht->vx0, old_vy0 = sht->vy0;
    sht->vx0 = vx0;
    sht->vy0 = vy0;
    if (sht->height >= 0) {
        /* 重新生成受影响区域的 map 并刷新 */
        sheet_refreshmap(sht->ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0);
        sheet_refreshmap(sht->ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);
        sheet_refreshsub(sht->ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0, sht->height - 1);
        sheet_refreshsub(sht->ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height, sht->height);
    }
}

void sheet_free(struct SHEET *sht) {
    if (sht->height >= 0) {
        sheet_updown(sht, -1);
    }
    sht->flags = 0;
}

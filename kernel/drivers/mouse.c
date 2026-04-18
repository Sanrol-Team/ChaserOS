/* kernel/drivers/mouse.c - PS/2 鼠标驱动实现 */

#include "mouse.h"
#include "../io.h"
#include "gui.h"
#include <stddef.h>

#define KEYCMD_SENDTO_MOUSE 0xd4
#define MOUSECMD_ENABLE     0xf4
#define KBC_STATUS_IBF      0x02
#define PORT_KEYDAT         0x0060
#define PORT_KEYCMD         0x0064

static mouse_dec_t mdec;

/* 等待键盘控制器就绪 */
static void wait_kbc_sendready() {
    while (inb(PORT_KEYCMD) & KBC_STATUS_IBF);
}

void mouse_init() {
    /* 启用鼠标端口 */
    wait_kbc_sendready();
    outb(PORT_KEYCMD, 0x60);
    wait_kbc_sendready();
    outb(PORT_KEYDAT, 0x47);

    /* 告知 KBC 下一个数据发往鼠标 */
    wait_kbc_sendready();
    outb(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_kbc_sendready();
    outb(PORT_KEYDAT, MOUSECMD_ENABLE);
    
    mdec.phase = 0;
}

int mouse_decode(mouse_dec_t *mdec, uint8_t dat) {
    if (mdec->phase == 0) {
        if (dat == 0xfa) mdec->phase = 1;
        return 0;
    }
    if (mdec->phase == 1) {
        if ((dat & 0xc8) == 0x08) {
            mdec->buf[0] = dat;
            mdec->phase = 2;
        }
        return 0;
    }
    if (mdec->phase == 2) {
        mdec->buf[1] = dat;
        mdec->phase = 3;
        return 0;
    }
    if (mdec->phase == 3) {
        mdec->buf[2] = dat;
        mdec->phase = 1;
        mdec->btn = mdec->buf[0] & 0x07;
        mdec->x = (int8_t)mdec->buf[1];
        mdec->y = (int8_t)mdec->buf[2];
        return 1;
    }
    return -1;
}

extern void gui_handle_mouse(int dx, int dy, int btn);

void mouse_handler() {
    uint8_t dat = inb(PORT_KEYDAT);
    if (mouse_decode(&mdec, dat) > 0) {
        /* 
         * 鼠标移动逻辑：
         * 注意：PS/2 鼠标返回的是相对位移。
         * y轴方向通常需要取反。
         */
        gui_handle_mouse(mdec.x, -mdec.y, mdec.btn);
    }
}

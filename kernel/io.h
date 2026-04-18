/* kernel/io.h - I/O 端口操作 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait() {
    /* 向不存在的端口写入数据以产生延迟 */
    outb(0x80, 0);
}

/* 端口 > 255 时必须用 DX，不能用 in/out 的 8 位立即数形式 */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %w1" : : "a"(val), "d"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %w1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %w1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %w1" : : "a"(val), "d"(port));
}

#endif

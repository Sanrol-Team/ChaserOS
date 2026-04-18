/* kernel/isr.c - 中断处理 C 程序 */

#include <stdint.h>
#include <stddef.h>
#include "io.h"
#include "shell.h"
#include "process.h"
#include "isr.h"
#include "user.h"
#include "syscall_abi.h"
#include "errno.h"
#include "vmm.h"

/* 键盘扫描码表 (简化版) */
static const char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

/* 外部函数声明 */
extern void puts(const char *s);
extern void puts_hex(uint64_t n);
extern void putchar(char c);

static void syscall_ret(struct registers *regs, int64_t value) {
    regs->rax = (uint64_t)value;
}

/* 中断处理主程序 */
void isr_handler(struct registers *regs) {
    if (regs->int_no < 32) {
        /* 处理异常 */
        puts("\n!!! CPU EXCEPTION !!!\n");
        puts("Exception Number: ");
        char hex[] = "0123456789ABCDEF";
        putchar(hex[regs->int_no / 16]);
        putchar(hex[regs->int_no % 16]);
        puts("\nError Code: ");
        puts_hex(regs->err_code);
        puts("\nRIP: ");
        puts_hex(regs->rip);
        puts("\nCS: ");
        puts_hex(regs->cs);
        puts("\nStopping system...\n");
        
        while (1) {
            __asm__ volatile("hlt");
        }
    } else if (regs->int_no >= 32 && regs->int_no <= 47) {
        /* 处理 IRQ */
        if (regs->int_no == 32) { /* IRQ 0: 时钟 */
            schedule();
        } else if (regs->int_no == 33) { /* IRQ 1: 键盘 */
            uint8_t scancode = inb(0x60);
            /* 只处理按下 (按下 < 0x80) */
            if (scancode < 0x80) {
                if (scancode < sizeof(scancode_ascii)) {
                    char c = scancode_ascii[scancode];
                    if (c) {
                        shell_handle_input(c);
                    }
                }
            }
        } else if (regs->int_no == 44) { /* IRQ 12: PS/2 鼠标（无 GUI 时排空数据） */
            (void)inb(0x60);
        }

        /* 发送 EOI (End of Interrupt) */
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20); /* 发送到从片 */
        }
        outb(0x20, 0x20);     /* 发送到主片 */
    } else if (regs->int_no == 128) {
        /* int $0x80：仅 ring 3 视为合法用户系统调用；否则拒绝并返回 -EPERM */
        if ((regs->cs & 3u) != 3u) {
            puts("\n[System call from kernel CPL0/CPL1/CPL2 — denied]\n");
            syscall_ret(regs, -EPERM);
            return;
        }

        switch (regs->rax) {
        case CNOS_SYS_EXIT:
            user_on_syscall_exit(regs);
            break;

        case CNOS_SYS_WRITE: {
            int fd = (int)regs->rdi;
            uint64_t buf_addr = regs->rsi;
            size_t len = (size_t)regs->rdx;

            if (fd != 1) {
                syscall_ret(regs, -EBADF);
                break;
            }
            if (len == 0) {
                syscall_ret(regs, 0);
                break;
            }
            if (len > CNOS_SYSCALL_MAX_WRITE_BYTES) {
                syscall_ret(regs, -EINVAL);
                break;
            }
            if (!vmm_user_range_readable(buf_addr, len)) {
                syscall_ret(regs, -EFAULT);
                break;
            }

            const char *buf = (const char *)buf_addr;
            for (size_t i = 0; i < len; i++) {
                putchar(buf[i]);
            }
            syscall_ret(regs, (int64_t)len);
            break;
        }

        default:
            syscall_ret(regs, -ENOSYS);
            break;
        }
    }
}

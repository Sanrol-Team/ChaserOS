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
#include "clock.h"
#include "user_fd.h"
#include "ipc.h"
#include "sched.h"

volatile uint64_t chaseros_kernel_ticks = 0;

static const char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

extern void puts(const char *s);
extern void puts_hex(uint64_t n);
extern void putchar(char c);

static void syscall_ret(struct registers *regs, int64_t value) {
    regs->rax = (uint64_t)value;
}

static void user_copy_message_in(uint64_t user_va, message_t *out) {
    uint8_t *p = (uint8_t *)user_va;
    uint8_t *dst = (uint8_t *)out;
    for (size_t i = 0; i < sizeof(message_t); i++) {
        dst[i] = p[i];
    }
}

static void user_copy_message_out(const message_t *in, uint64_t user_va) {
    const uint8_t *src = (const uint8_t *)in;
    uint8_t *p = (uint8_t *)user_va;
    for (size_t i = 0; i < sizeof(message_t); i++) {
        p[i] = src[i];
    }
}

void isr_handler(struct registers *regs) {
    if (regs->int_no < 32) {
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
        if (regs->int_no == 14u) {
            uint64_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            puts("\nCR2 (fault VA): ");
            puts_hex(cr2);
        }
        puts("\nStopping system...\n");

        while (1) {
            __asm__ volatile("hlt");
        }
    } else if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no == 32) {
            chaseros_kernel_ticks++;
            sched_on_timer_tick();
        } else if (regs->int_no == 33) {
            uint8_t scancode = inb(0x60);
            if (scancode < 0x80) {
                if (scancode < sizeof(scancode_ascii)) {
                    char c = scancode_ascii[scancode];
                    if (c) {
                        shell_handle_input(c);
                    }
                }
            }
        } else if (regs->int_no == 44) {
            (void)inb(0x60);
        }

        if (regs->int_no >= 40) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    } else if (regs->int_no == 128) {
        if ((regs->cs & 3u) != 3u) {
            puts("\n[System call from kernel CPL0/CPL1/CPL2 — denied]\n");
            syscall_ret(regs, -EPERM);
            return;
        }

        switch (regs->rax) {
        case CHASEROS_SYS_EXIT:
            user_on_syscall_exit(regs);
            break;

        case CHASEROS_SYS_WRITE: {
            int fd = (int)regs->rdi;
            uint64_t buf_addr = regs->rsi;
            size_t len = (size_t)regs->rdx;

            if (fd != 1 && fd != 2) {
                syscall_ret(regs, -EBADF);
                break;
            }
            if (len == 0) {
                syscall_ret(regs, 0);
                break;
            }
            if (len > CHASEROS_SYSCALL_MAX_IO_BYTES) {
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

        case CHASEROS_SYS_GETPID:
            syscall_ret(regs, (int64_t)chaseros_active_user_pid);
            break;

        case CHASEROS_SYS_UPTIME_TICKS:
            syscall_ret(regs, (int64_t)chaseros_kernel_ticks);
            break;

        case CHASEROS_SYS_READ: {
            int fd = (int)regs->rdi;
            uint64_t buf_addr = regs->rsi;
            size_t len = (size_t)regs->rdx;

            if (fd == 0) {
                if (len == 0) {
                    syscall_ret(regs, 0);
                    break;
                }
                if (len > CHASEROS_SYSCALL_MAX_IO_BYTES) {
                    syscall_ret(regs, -EINVAL);
                    break;
                }
                if (!vmm_user_range_writable(buf_addr, len)) {
                    syscall_ret(regs, -EFAULT);
                    break;
                }
                syscall_ret(regs, 0);
                break;
            }
            if (fd == 1 || fd == 2) {
                syscall_ret(regs, -EBADF);
                break;
            }
            if (fd >= 3) {
                syscall_ret(regs, user_fd_sys_read(fd, buf_addr, len));
                break;
            }
            syscall_ret(regs, -EBADF);
            break;
        }

        case CHASEROS_SYS_OPEN:
            syscall_ret(regs, user_fd_sys_open(regs->rdi, (int)regs->rsi));
            break;

        case CHASEROS_SYS_CLOSE:
            syscall_ret(regs, user_fd_sys_close((int)regs->rdi));
            break;

        case CHASEROS_SYS_IPC_SEND: {
            uint64_t dest_pid = regs->rdi;
            uint64_t msg_uva = regs->rsi;
            if (!vmm_user_range_readable(msg_uva, sizeof(message_t))) {
                syscall_ret(regs, -EFAULT);
                break;
            }
            message_t m;
            user_copy_message_in(msg_uva, &m);
            int r = ipc_send(dest_pid, &m);
            syscall_ret(regs, r == IPC_OK ? 0 : -EINVAL);
            break;
        }

        case CHASEROS_SYS_IPC_RECV: {
            uint64_t src_pid = regs->rdi;
            uint64_t msg_uva = regs->rsi;
            if (!vmm_user_range_writable(msg_uva, sizeof(message_t))) {
                syscall_ret(regs, -EFAULT);
                break;
            }
            message_t m;
            int r = ipc_receive(src_pid, &m);
            if (r == IPC_OK) {
                user_copy_message_out(&m, msg_uva);
                syscall_ret(regs, 0);
            } else {
                syscall_ret(regs, -EINVAL);
            }
            break;
        }

        case CHASEROS_SYS_IPC_REPLY: {
            uint64_t dest_pid = regs->rdi;
            uint64_t msg_uva = regs->rsi;
            if (!vmm_user_range_readable(msg_uva, sizeof(message_t))) {
                syscall_ret(regs, -EFAULT);
                break;
            }
            message_t m;
            user_copy_message_in(msg_uva, &m);
            int r = ipc_reply(dest_pid, &m);
            syscall_ret(regs, r == IPC_OK ? 0 : -EINVAL);
            break;
        }

        case CHASEROS_SYS_IPC_CALL: {
            uint64_t dest_pid = regs->rdi;
            uint64_t req_u = regs->rsi;
            uint64_t rep_u = regs->rdx;
            if (!vmm_user_range_readable(req_u, sizeof(message_t)) ||
                !vmm_user_range_writable(rep_u, sizeof(message_t))) {
                syscall_ret(regs, -EFAULT);
                break;
            }
            message_t req, rep;
            user_copy_message_in(req_u, &req);
            if (ipc_send(dest_pid, &req) != IPC_OK) {
                syscall_ret(regs, -EINVAL);
                break;
            }
            proc_t *cur = process_current_task_for_ipc();
            ipc_wait_until_pending((struct proc *)cur);
            ipc_consume_pending_message((struct proc *)cur, &rep);
            user_copy_message_out(&rep, rep_u);
            syscall_ret(regs, 0);
            break;
        }

        default:
            syscall_ret(regs, -ENOSYS);
            break;
        }
    }
}

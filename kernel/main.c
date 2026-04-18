/* kernel/main.c - 微内核主要入口点 */

#include <stdint.h>
#include "pmm.h"
#include "vmm.h"
#include "idt.h"
#include "shell.h"
#include "process.h"
#include "multiboot.h"
#include "fs/vfs.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "console.h"
#include "drivers/ide.h"
#include "fs/cnos/cnos_ext2_vol.h"
#include "gdt.h"
#include "hybrid_ipc.h"

void putchar(char c) {
    serial_putchar(c);
    console_putchar(c);
}

void puts(const char *s) {
    serial_puts(s);
    console_puts(s);
}

void clear_screen(void) {
    console_clear();
}

void puts_hex(uint64_t n) {
    char hex[] = "0123456789ABCDEF";
    puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        putchar(hex[(n >> i) & 0xF]);
    }
}

void puts_dec(uint64_t n) {
    if (n == 0) {
        putchar('0');
        return;
    }
    char buf[20];
    int i = 0;
    while (n > 0) {
        buf[i++] = (char)((n % 10) + '0');
        n /= 10;
    }
    while (--i >= 0) {
        putchar(buf[i]);
    }
}

void kernel_main(uint64_t mbi_phys, uint64_t boot_magic) {
    serial_init();

    if (!multiboot_validate((uint32_t)boot_magic, mbi_phys)) {
        vga_init();
        const char *err = "CNOS: invalid Multiboot2 handoff\n";
        serial_puts(err);
        vga_puts(err);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    console_init(mbi_phys);

    pmm_init(mbi_phys);
    vmm_init();
    ide_init();
    cnos_vol_init();
    vfs_init();

    process_init();
    gdt_init();
    idt_init();
    shell_init();

    cnos_hybrid_ipc_service_spawn();

    puts("Welcome to CNOS hybrid kernel (IPC FS service PID=");
    puts_dec(CNOS_HYBRID_SERVICE_PID);
    puts(", serial + VGA shell).\n");
    puts("Memory OK. VFS init done.\n\nMultiboot2 @ ");
    puts_hex(mbi_phys);
    puts("\nPhysical: ");
    puts_dec(pmm_get_total_memory() / 1024 / 1024);
    puts(" MB total, ");
    puts_dec(pmm_get_free_memory() / 1024 / 1024);
    puts(" MB free\n");

    __asm__ volatile("sti");
    puts("\nCNOS> ");

    for (;;) {
        __asm__ volatile("hlt");
    }
}

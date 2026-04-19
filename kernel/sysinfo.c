/* kernel/sysinfo.c */

#include "sysinfo.h"
#include "console.h"
#include "drivers/ide.h"
#include "sched.h"

extern void puts(const char *s);
extern void puts_dec(uint64_t n);
extern void puts_hex(uint64_t n);
extern void putchar(char c);

#include "pmm.h"

void sysinfo_print(void) {
    puts("ChaserOS System Information\n");

    puts("  Kernel: hybrid (in-kernel services + IRQ0 RR + ring3 slot)\n");
    puts("  Scheduler context switches: ");
    puts_dec(sched_context_switches());
    puts("\n");

    puts("  Multiboot2 MBI (phys): ");
    puts_hex(console_get_mbi_phys());
    puts("\n");

    puts("  RAM total: ");
    puts_dec(pmm_get_total_memory() / (1024 * 1024));
    puts(" MB\n  RAM free:  ");
    puts_dec(pmm_get_free_memory() / (1024 * 1024));
    puts(" MB\n");

    if (console_is_framebuffer()) {
        int w = 0, h = 0;
        console_fb_dims(&w, &h);
        puts("  Display: framebuffer ");
        puts_dec((uint64_t)(unsigned)w);
        puts(" x ");
        puts_dec((uint64_t)(unsigned)h);
        puts("\n");
    } else {
        puts("  Display: VGA text 80x25\n");
    }

    puts("  Font (shell): ");
    puts(console_font_desc());
    puts("\n");

    puts("  IDE primary channel:\n");
    char m[40];
    int cls;
    for (unsigned d = 0; d < 2u; d++) {
        puts("    ");
        puts_dec((uint64_t)d);
        puts(": ");
        if (ide_probe_type((uint8_t)d, m, &cls) != 0) {
            puts("(no device)\n");
            continue;
        }
        if (cls == IDE_CLASS_ATA) {
            puts("[ATA HDD] ");
        } else if (cls == IDE_CLASS_ATAPI) {
            puts("[CD-ROM ATAPI] ");
        }
        for (int k = 0; k < 40; k++) {
            char ch = m[k];
            if (ch >= 32 && ch < 127) {
                putchar(ch);
            }
        }
        putchar('\n');
    }

    puts("  CD-ROM: use Shell commands atapi inquiry <0|1> and atapi read <0|1> <lba> (READ(10), 2048 bytes/sector).\n");
}

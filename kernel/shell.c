/* kernel/shell.c - 基础 Shell + 卷 / ext2 / IDE 命令 */

#include "shell.h"
#include "drivers/vga.h"
#include "drivers/pci.h"
#include "drivers/ide.h"
#include "fs/cnos/cnos_ext2_vol.h"
#include "fs/vfs.h"
#include "user.h"
#include "power.h"
#include "console.h"
#include "sysinfo.h"
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>

#ifndef CNRUN_MAX_BYTES
#define CNRUN_MAX_BYTES (2u * 1024u * 1024u)
#endif

extern void puts(const char *s);
extern void putchar(char c);
extern void vga_clear();
extern void puts_dec(uint64_t n);

static char cmd_buffer[MAX_COMMAND_LEN];
static int cmd_len = 0;

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static size_t strlen_local(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static void skip_sp(const char **p) {
    while (**p == ' ') {
        (*p)++;
    }
}

static int parse_u64(const char *s, uint64_t *out) {
    *out = 0;
    if (!s || !*s) {
        return -1;
    }
    while (*s) {
        if (*s < '0' || *s > '9') {
            return -1;
        }
        *out = *out * 10ull + (uint64_t)(*s - '0');
        s++;
    }
    return 0;
}

static int strncmp_local(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) {
        a++;
        b++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(unsigned char *)a - *(unsigned char *)b;
}

void shell_init() {
    for (int i = 0; i < MAX_COMMAND_LEN; i++) {
        cmd_buffer[i] = 0;
    }
    cmd_len = 0;
}

void shell_handle_input(char c) {
    if (c == '\n' || c == '\r') {
        cmd_buffer[cmd_len] = '\0';
        putchar('\n');
        shell_execute(cmd_buffer);
        cmd_len = 0;
        cmd_buffer[0] = '\0';
        puts("CNOS> ");
        return;
    }
    if (c == '\b' || c == 127) {
        if (cmd_len > 0) {
            cmd_len--;
            cmd_buffer[cmd_len] = '\0';
            putchar('\b');
            putchar(' ');
            putchar('\b');
        }
        return;
    }
    if (cmd_len < MAX_COMMAND_LEN - 1 && c >= 32 && c < 127) {
        cmd_buffer[cmd_len++] = c;
        cmd_buffer[cmd_len] = '\0';
        putchar(c);
    }
}

void shell_execute(const char *cmd) {
    char buf[MAX_COMMAND_LEN];
    size_t i = 0;
    while (cmd[i] && i < MAX_COMMAND_LEN - 1) {
        buf[i] = cmd[i];
        i++;
    }
    buf[i] = '\0';

    const char *p = buf;
    skip_sp(&p);
    if (*p == '\0') {
        return;
    }

    char verb[32];
    int vi = 0;
    while (*p && *p != ' ' && vi < (int)sizeof(verb) - 1) {
        verb[vi++] = *p++;
    }
    verb[vi] = '\0';
    skip_sp(&p);

    if (strcmp(verb, "help") == 0) {
        puts("Commands:\n");
        puts("  help              - This help\n");
        puts("  clear             - Clear screen\n");
        puts("  mem               - Memory stats\n");
        puts("  reboot            - Reboot (ACPI/ICH/8042)\n");
        puts("  poweroff          - Soft power off (ACPI / QEMU, kernel I/O)\n");
        puts("  shutdown          - Same as poweroff\n");
        puts("  mkdisk <sectors>  - RAM disk (512B sectors, even count, min 512)\n");
        puts("  vol               - Show current volume + VFS mount state\n");
        puts("  mount             - Mount current volume as VFS root (ext2)\n");
        puts("  umount            - Unmount VFS root (volume stays attached)\n");
        puts("  format            - Create minimal ext2 on current volume\n");
        puts("  ls                - List root via VFS (needs mount)\n");
        puts("  read <name>       - Read file via VFS (needs mount)\n");
        puts("  write <name> <text> - Write file via VFS (needs mount)\n");
        puts("  ide               - IDENTIFY primary IDE master/slave\n");
        puts("  attach ide <0|1>  - Use IDE master/slave as volume\n");
        puts("  detach            - Unmount VFS if needed, release volume\n");
        puts("  lspci             - PCI devices\n");
        puts("  ps                - Fake process list\n");
        puts("  hello             - Run embedded ring-3 user demo (C)\n");
        puts("  cnrun             - Run embedded CNAF demo (MANIFEST + hello.elf IMAGE)\n");
        puts("  cnrun <file>      - Run .cnaf from VFS root (needs mount; max ~2 MiB)\n");
        puts("  slime             - Run embedded Slime user demo (requires CNOS_WITH_SLIME_USER)\n");
        puts("  systeminfo        - OS / RAM / display / font / IDE / CD-ROM\n");
    } else if (strcmp(verb, "clear") == 0) {
        console_clear();
    } else if (strcmp(verb, "mem") == 0) {
        puts("Memory Statistics:\n");
        puts("  Total: ");
        puts_dec(pmm_get_total_memory() / 1024 / 1024);
        puts(" MB\n");
        puts("  Used:  ");
        puts_dec(pmm_get_used_memory() / 1024);
        puts(" KB\n");
        puts("  Free:  ");
        puts_dec(pmm_get_free_memory() / 1024 / 1024);
        puts(" MB\n");
    } else if (strcmp(verb, "reboot") == 0) {
        puts("Rebooting...\n");
        power_reboot();
    } else if (strcmp(verb, "poweroff") == 0 || strcmp(verb, "shutdown") == 0) {
        puts("Powering off...\n");
        power_shutdown();
    } else if (strcmp(verb, "mkdisk") == 0) {
        uint64_t sectors = 0;
        if (parse_u64(p, &sectors) != 0 || sectors < 512u || (sectors % 2u) != 0u) {
            puts("mkdisk: usage mkdisk <sectors> (even, >=512)\n");
            return;
        }
        uint64_t bytes = sectors * 512ull;
        uint64_t pages = (bytes + (uint64_t)PAGE_SIZE - 1ull) / (uint64_t)PAGE_SIZE;
        void *mem = pmm_alloc_contiguous_pages(pages);
        if (!mem) {
            puts("mkdisk: allocation failed\n");
            return;
        }
        if (cnos_vol_attach_ram(mem, (size_t)bytes) != 0) {
            pmm_free_contiguous(mem, pages);
            puts("mkdisk: attach failed\n");
            return;
        }
        puts("mkdisk: RAM volume ");
        puts_dec(bytes);
        puts(" bytes");
        if (vfs_mount_root() == VFS_ERR_NONE) {
            puts(", VFS mounted\n");
        } else {
            puts("\n");
        }
    } else if (strcmp(verb, "vol") == 0) {
        const cnos_vol_t *v = cnos_vol_current();
        if (!v) {
            puts("No volume attached.\n");
            puts("VFS root: not mounted\n");
            return;
        }
        if (v->type == CNOS_VOL_RAM) {
            puts("Volume: RAM, ");
            puts_dec((uint64_t)v->size_bytes);
            puts(" bytes\n");
        } else if (v->type == CNOS_VOL_IDE) {
            puts("Volume: IDE ");
            puts_dec((uint64_t)v->ide_drive);
            puts(", ");
            puts_dec((uint64_t)v->size_bytes);
            puts(" bytes\n");
        }
        puts("VFS root: ");
        puts(vfs_is_mounted() ? "mounted (ext2)\n" : "not mounted\n");
    } else if (strcmp(verb, "mount") == 0) {
        int mr = vfs_mount_root();
        if (mr == VFS_ERR_NONE) {
            puts("mount: ext2 root ok\n");
        } else if (mr == VFS_ERR_NOENT) {
            puts("mount: no volume (mkdisk / attach ide first)\n");
        } else {
            puts("mount: failed\n");
        }
    } else if (strcmp(verb, "umount") == 0) {
        vfs_umount_root();
        puts("umount: ok\n");
    } else if (strcmp(verb, "detach") == 0) {
        vfs_umount_root();
        cnos_vol_detach_all();
        puts("Volume detached.\n");
    } else if (strcmp(verb, "format") == 0) {
        int fr = vfs_format();
        if (fr == VFS_ERR_NOENT) {
            puts("format: no volume\n");
            return;
        }
        if (fr != VFS_ERR_NONE) {
            puts("format: failed\n");
            return;
        }
        puts("format: ext2 created (single group, 1024-byte blocks)\n");
    } else if (strcmp(verb, "ls") == 0) {
        int lr = vfs_ls();
        if (lr == VFS_ERR_NOTMOUNTED) {
            puts("ls: VFS not mounted (try: mount)\n");
        } else if (lr != VFS_ERR_NONE) {
            puts("ls: failed\n");
        }
    } else if (strcmp(verb, "read") == 0) {
        skip_sp(&p);
        if (*p == '\0') {
            puts("read: usage read <name>\n");
            return;
        }
        char name[128];
        int ni = 0;
        while (*p && *p != ' ' && ni < 127) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        char out[4096];
        size_t n = 0;
        int rr = vfs_read_file(name, out, sizeof out, &n);
        if (rr == VFS_ERR_NOTMOUNTED) {
            puts("read: VFS not mounted (try: mount)\n");
            return;
        }
        if (rr != VFS_ERR_NONE) {
            puts("read: not found or not a regular file\n");
            return;
        }
        size_t j;
        for (j = 0; j < n; j++) {
            char c = out[j];
            if (c >= 32 && c < 127) {
                putchar(c);
            } else {
                putchar('.');
            }
        }
        putchar('\n');
    } else if (strcmp(verb, "write") == 0) {
        skip_sp(&p);
        char name[128];
        int ni = 0;
        while (*p && *p != ' ' && ni < 127) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        skip_sp(&p);
        if (name[0] == '\0') {
            puts("write: usage write <name> <text...>\n");
            return;
        }
        int wr = vfs_write_file(name, p, strlen_local(p));
        if (wr == VFS_ERR_NOTMOUNTED) {
            puts("write: VFS not mounted (try: mount)\n");
            return;
        }
        if (wr != VFS_ERR_NONE) {
            puts("write: failed\n");
            return;
        }
        puts("write: ok\n");
    } else if (strcmp(verb, "ide") == 0) {
        char m[40];
        puts("IDE primary channel:\n");
        for (int d = 0; d < 2; d++) {
            puts("  ");
            puts_dec((uint64_t)d);
            puts(": ");
            if (ide_identify((uint8_t)d, m) != 0) {
                puts("(no device)\n");
            } else {
                int k;
                for (k = 0; k < 40 && m[k] != ' '; k++) {
                    putchar(m[k]);
                }
                putchar('\n');
            }
        }
    } else if (strcmp(verb, "attach") == 0) {
        if (strncmp_local(p, "ide", 3) == 0 && (p[3] == ' ' || p[3] == '\0')) {
            p += 3;
            skip_sp(&p);
            uint64_t d = 0;
            if (parse_u64(p, &d) != 0 || (d != 0 && d != 1)) {
                puts("attach: usage attach ide <0|1>\n");
                return;
            }
            if (cnos_vol_attach_ide((uint8_t)d) != 0) {
                puts("attach: IDE failed (no disk or size too small)\n");
                return;
            }
            puts("attach: IDE volume ready");
            if (vfs_mount_root() == VFS_ERR_NONE) {
                puts(", VFS mounted\n");
            } else {
                puts("\n");
            }
        } else {
            puts("attach: only 'attach ide <0|1>' supported\n");
        }
    } else if (strcmp(verb, "lspci") == 0) {
        pci_list_devices();
    } else if (strcmp(verb, "ps") == 0) {
        puts("Simulation Process List:\n");
        puts(" PID | State | Name\n");
        puts("-----|-------|------\n");
        puts(" 0   | R     | Kernel\n");
        puts(" 1   | R     | Shell\n");
    } else if (strcmp(verb, "hello") == 0) {
        user_run_embedded_hello();
    } else if (strcmp(verb, "cnrun") == 0) {
        skip_sp(&p);
        if (*p == '\0') {
            user_run_embedded_demo_cnaf();
            return;
        }
        char name[128];
        int ni = 0;
        while (*p && *p != ' ' && ni < 127) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        vfs_stat_t st;
        int sr = vfs_stat(name, &st);
        if (sr == VFS_ERR_NOTMOUNTED) {
            puts("cnrun: VFS not mounted (try: mount)\n");
            return;
        }
        if (sr != VFS_ERR_NONE) {
            puts("cnrun: file not found\n");
            return;
        }
        if (st.size == 0 || st.size > CNRUN_MAX_BYTES) {
            puts("cnrun: empty or too large\n");
            return;
        }
        uint64_t pages64 = ((uint64_t)st.size + (uint64_t)PAGE_SIZE - 1ULL) / (uint64_t)PAGE_SIZE;
        uint8_t *blob = (uint8_t *)pmm_alloc_contiguous_pages(pages64);
        if (!blob) {
            puts("cnrun: alloc failed\n");
            return;
        }
        size_t got = 0;
        int rr = vfs_read_file_range(name, 0u, blob, (size_t)st.size, &got);
        if (rr != VFS_ERR_NONE || got != st.size) {
            pmm_free_contiguous(blob, pages64);
            puts("cnrun: read failed\n");
            return;
        }
        user_run_cnaf_from_owned_pages(blob, got, pages64, name);
    } else if (strcmp(verb, "slime") == 0) {
        user_run_embedded_slime_hello();
    } else if (strcmp(verb, "systeminfo") == 0) {
        sysinfo_print();
    } else if (cmd[0] != '\0') {
        puts("Unknown command: ");
        puts(cmd);
        puts("\n");
    }
}

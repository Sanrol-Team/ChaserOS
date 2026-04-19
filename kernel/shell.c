/* kernel/shell.c - 基础 Shell + 卷 / ext2 / IDE 命令 */

#include "shell.h"
#include "drivers/vga.h"
#include "drivers/pci.h"
#include "drivers/ide.h"
#include "fs/chaseros/chaseros_ext2_vol.h"
#include "fs/vfs.h"
#include "user.h"
#include "power.h"
#include "console.h"
#include "sysinfo.h"
#include "sched.h"
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

/** Shell 当前目录（绝对路径，已规范化） */
static char g_cwd[256] = "/";

static void shell_cwd_reset(void) {
    g_cwd[0] = '/';
    g_cwd[1] = '\0';
    chaseros_user_cwd_reset();
}

const char *shell_get_cwd(void) {
    return g_cwd;
}

static int shell_resolve(const char *rel, char *out, size_t outsz) {
    return vfs_resolve_path(g_cwd, rel, out, outsz);
}

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
    shell_cwd_reset();
}

void shell_handle_input(char c) {
    if (c == '\n' || c == '\r') {
        cmd_buffer[cmd_len] = '\0';
        putchar('\n');
        shell_execute(cmd_buffer);
        cmd_len = 0;
        cmd_buffer[0] = '\0';
        puts("ChaserOS> ");
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

static void shell_resolution_cmd(const char *args) {
    const char *r = args;
    skip_sp(&r);
    if (*r == '\0') {
        if (!console_is_framebuffer()) {
            puts("resolution: VGA text — 像素模式请编辑 iso/boot/grub.cfg 中 set gfxpayload= 后重建 ISO 重启。\n");
            puts("  帧缓冲模式下: resolution scale <1-10> | resolution auto | resolution help\n");
            return;
        }
        int w = 0, h = 0;
        console_fb_dims(&w, &h);
        puts("Framebuffer: ");
        puts_dec((uint64_t)(unsigned)w);
        puts(" x ");
        puts_dec((uint64_t)(unsigned)h);
        puts("  font scale: ");
        puts_dec((uint64_t)(unsigned)console_get_fb_font_scale());
        puts(" (1-10；档位非硬件像素，见 resolution help)\n");
        return;
    }
    char w0[20];
    int wi = 0;
    while (*r && *r != ' ' && wi < (int)sizeof(w0) - 1) {
        w0[wi++] = *r++;
    }
    w0[wi] = '\0';
    skip_sp(&r);
    if (strcmp(w0, "help") == 0) {
        puts("resolution              当前像素与字模档位\n");
        puts("resolution auto         字模按屏高自动\n");
        puts("resolution scale <1-10> 字模变大/变小（位图字体；矢量模块启用时无效）\n");
        puts("硬件分辨率：改 iso/boot/grub.cfg 的 set gfxpayload=WxHx32，cmake --build 后换 ISO 重启。\n");
        return;
    }
    if (strcmp(w0, "auto") == 0) {
        console_set_fb_font_scale(0);
        puts("resolution: font scale auto\n");
        return;
    }
    if (strcmp(w0, "scale") == 0) {
        uint64_t sc = 0;
        if (parse_u64(r, &sc) != 0 || sc < 1ull || sc > 10ull) {
            puts("resolution: usage: resolution scale <1-10>\n");
            return;
        }
        console_set_fb_font_scale((int)sc);
        puts("resolution: font scale ");
        puts_dec(sc);
        puts("\n");
        return;
    }
    puts("resolution: unknown subcommand (try resolution help)\n");
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
        puts("  initdisk          - format + mount fresh ext2 (resets cwd to /)\n");
        puts("  vol               - Show current volume + VFS mount state\n");
        puts("  mount             - Mount current volume as VFS root (ext2)\n");
        puts("  umount            - Unmount VFS root (volume stays attached)\n");
        puts("  format            - Create minimal ext2 on current volume\n");
        puts("  pwd               - Print current directory\n");
        puts("  cd [path]         - Change directory (default /)\n");
        puts("  gfl [path]        - Same as cd (go to directory)\n");
        puts("  mkdir <path>      - Create subdirectory under cwd or absolute path\n");
        puts("  ls [path]         - List directory (default: cwd)\n");
        puts("  read <name>       - Read file (cwd-relative or absolute)\n");
        puts("  write <name> <text> - Write file (cwd-relative or absolute)\n");
        puts("  ide               - Probe ATA/ATAPI on primary IDE master/slave\n");
        puts("  atapi inquiry <0|1> - SCSI INQUIRY (CD-ROM ATAPI)\n");
        puts("  atapi read <0|1> <lba> - READ(10) one 2048-byte sector (hex dump head)\n");
        puts("  attach ide <0|1>  - Use IDE master/slave as volume\n");
#ifdef CHASEROS_HAVE_AHCI_RUST
        puts("  attach ahci <0-31> - Use AHCI SATA port as volume (AHCI build)\n");
#endif
        puts("  detach            - Unmount VFS if needed, release volume\n");
        puts("  lspci             - PCI devices\n");
        puts("  ps                - Hybrid scheduler task table + stats\n");
        puts("  hello             - Run embedded ring-3 user demo (C)\n");
        puts("  cnrun             - Run embedded CNAF (no args; IMAGE: C hello or Slime CNAFLOADER)\n");
        puts("  cnrun <file>      - Run .cnaf (cwd-relative or absolute; max ~2 MiB)\n");
        puts("  slime             - Run embedded Slime user demo (requires CHASEROS_WITH_SLIME_USER)\n");
        puts("  systeminfo        - OS / RAM / display / font / IDE / CD-ROM\n");
        puts("  resolution [help|auto|scale N] - 帧缓冲下调整字模档位；像素模式改 grub.cfg\n");
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
        if (chaseros_vol_attach_ram(mem, (size_t)bytes) != 0) {
            pmm_free_contiguous(mem, pages);
            puts("mkdisk: attach failed\n");
            return;
        }
        shell_cwd_reset();
        puts("mkdisk: RAM volume ");
        puts_dec(bytes);
        puts(" bytes");
        if (vfs_mount_root() == VFS_ERR_NONE) {
            puts(", VFS mounted\n");
        } else {
            puts("\n");
        }
    } else if (strcmp(verb, "vol") == 0) {
        const chaseros_vol_t *v = chaseros_vol_current();
        if (!v) {
            puts("No volume attached.\n");
            puts("VFS root: not mounted\n");
            return;
        }
        if (v->type == CHASEROS_VOL_RAM) {
            puts("Volume: RAM, ");
            puts_dec((uint64_t)v->size_bytes);
            puts(" bytes\n");
        } else if (v->type == CHASEROS_VOL_IDE) {
            puts("Volume: IDE ");
            puts_dec((uint64_t)v->ide_drive);
            puts(", ");
            puts_dec((uint64_t)v->size_bytes);
            puts(" bytes\n");
        } else if (v->type == CHASEROS_VOL_AHCI) {
            puts("Volume: AHCI port ");
            puts_dec((uint64_t)v->ahci_port);
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
#ifdef CHASEROS_HAVE_AHCI_RUST
            puts("mount: no volume (mkdisk / attach ide / attach ahci first)\n");
#else
            puts("mount: no volume (mkdisk / attach ide first)\n");
#endif
        } else {
            puts("mount: failed\n");
        }
    } else if (strcmp(verb, "umount") == 0) {
        vfs_umount_root();
        shell_cwd_reset();
        puts("umount: ok\n");
    } else if (strcmp(verb, "detach") == 0) {
        vfs_umount_root();
        shell_cwd_reset();
        chaseros_vol_detach_all();
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
        shell_cwd_reset();
        puts("format: ext2 created (single group, 1024-byte blocks)\n");
    } else if (strcmp(verb, "initdisk") == 0) {
        vfs_umount_root();
        shell_cwd_reset();
        int fr = vfs_format();
        if (fr == VFS_ERR_NOENT) {
#ifdef CHASEROS_HAVE_AHCI_RUST
            puts("initdisk: no volume (mkdisk / attach ide / attach ahci first)\n");
#else
            puts("initdisk: no volume (mkdisk / attach ide first)\n");
#endif
            return;
        }
        if (fr != VFS_ERR_NONE) {
            puts("initdisk: format failed\n");
            return;
        }
        int mr = vfs_mount_root();
        if (mr != VFS_ERR_NONE) {
            puts("initdisk: mount failed\n");
            return;
        }
        puts("initdisk: volume formatted (ext2) and mounted; cwd=/\n");
    } else if (strcmp(verb, "pwd") == 0) {
        puts(g_cwd);
        puts("\n");
    } else if (strcmp(verb, "cd") == 0 || strcmp(verb, "gfl") == 0) {
        const char *cderr = (strcmp(verb, "gfl") == 0) ? "gfl" : "cd";
        skip_sp(&p);
        char resolved[256];
        if (*p == '\0') {
            shell_cwd_reset();
            return;
        }
        if (shell_resolve(p, resolved, sizeof resolved) != 0) {
            puts(cderr);
            puts(": invalid path\n");
            return;
        }
        if (!vfs_is_mounted()) {
            puts(cderr);
            puts(": VFS not mounted (try: mount)\n");
            return;
        }
        if (!vfs_is_directory(resolved)) {
            puts(cderr);
            puts(": not a directory\n");
            return;
        }
        size_t ci = 0;
        while (resolved[ci] && ci + 1 < sizeof g_cwd) {
            g_cwd[ci] = resolved[ci];
            ci++;
        }
        g_cwd[ci] = '\0';
        chaseros_user_set_cwd(g_cwd);
    } else if (strcmp(verb, "mkdir") == 0) {
        skip_sp(&p);
        if (*p == '\0') {
            puts("mkdir: usage mkdir <path>\n");
            return;
        }
        char resolved[256];
        if (shell_resolve(p, resolved, sizeof resolved) != 0) {
            puts("mkdir: invalid path\n");
            return;
        }
        int mr = vfs_mkdir(resolved);
        if (mr == VFS_ERR_NOTMOUNTED) {
            puts("mkdir: VFS not mounted (try: mount)\n");
            return;
        }
        if (mr != VFS_ERR_NONE) {
            puts("mkdir: failed (exists or disk full)\n");
            return;
        }
        puts("mkdir: ok\n");
    } else if (strcmp(verb, "ls") == 0) {
        skip_sp(&p);
        char resolved[256];
        const char *listpath = g_cwd;
        if (*p != '\0') {
            if (shell_resolve(p, resolved, sizeof resolved) != 0) {
                puts("ls: invalid path\n");
                return;
            }
            listpath = resolved;
        }
        int lr = vfs_ls(listpath);
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
        char resolved[256];
        if (shell_resolve(name, resolved, sizeof resolved) != 0) {
            puts("read: invalid path\n");
            return;
        }
        char out[4096];
        size_t n = 0;
        int rr = vfs_read_file(resolved, out, sizeof out, &n);
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
        char wresolved[256];
        if (shell_resolve(name, wresolved, sizeof wresolved) != 0) {
            puts("write: invalid path\n");
            return;
        }
        int wr = vfs_write_file(wresolved, p, strlen_local(p));
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
        int cls;
        puts("IDE primary channel:\n");
        for (int d = 0; d < 2; d++) {
            puts("  ");
            puts_dec((uint64_t)d);
            puts(": ");
            if (ide_probe_type((uint8_t)d, m, &cls) != 0) {
                puts("(no device)\n");
                continue;
            }
            if (cls == IDE_CLASS_ATA) {
                puts("[ATA] ");
            } else if (cls == IDE_CLASS_ATAPI) {
                puts("[ATAPI] ");
            } else {
                puts("[?] ");
            }
            int k;
            for (k = 0; k < 40 && m[k] != ' '; k++) {
                putchar(m[k]);
            }
            if (cls == IDE_CLASS_ATA) {
                uint32_t sec = 0;
                puts("  sectors=");
                if (ide_capacity_sectors((uint8_t)d, &sec) == 0) {
                    puts_dec((uint64_t)sec);
                } else {
                    puts("?");
                }
            }
            putchar('\n');
        }
    } else if (strcmp(verb, "atapi") == 0) {
        skip_sp(&p);
        char sub[16];
        int si = 0;
        while (*p && *p != ' ' && si < (int)sizeof(sub) - 1) {
            sub[si++] = *p++;
        }
        sub[si] = '\0';
        skip_sp(&p);
        if (strcmp(sub, "inquiry") == 0) {
            uint64_t dr = 0;
            if (parse_u64(p, &dr) != 0 || (dr != 0 && dr != 1)) {
                puts("atapi: usage atapi inquiry <0|1>\n");
                return;
            }
            int cls = IDE_CLASS_NONE;
            char m[40];
            if (ide_probe_type((uint8_t)dr, m, &cls) != 0 || cls != IDE_CLASS_ATAPI) {
                puts("atapi: drive is not ATAPI (use ide)\n");
                return;
            }
            uint8_t inq[96];
            uint32_t act = 0;
            if (ide_atapi_inquiry((uint8_t)dr, inq, sizeof inq, &act) != 0) {
                puts("atapi: INQUIRY failed\n");
                return;
            }
            puts("INQUIRY ok, bytes=");
            puts_dec((uint64_t)act);
            puts("\n  vendor: ");
            for (int k = 8; k < 16; k++) {
                char c = (char)inq[k];
                putchar((c >= 32 && c < 127) ? c : '?');
            }
            puts("\n  product: ");
            for (int k = 16; k < 32; k++) {
                char c = (char)inq[k];
                putchar((c >= 32 && c < 127) ? c : '?');
            }
            putchar('\n');
        } else if (strcmp(sub, "read") == 0) {
            uint64_t dr = 0;
            if (parse_u64(p, &dr) != 0 || (dr != 0 && dr != 1)) {
                puts("atapi: usage atapi read <0|1> <lba>\n");
                return;
            }
            while (*p && *p != ' ') {
                p++;
            }
            skip_sp(&p);
            uint64_t lba = 0;
            if (parse_u64(p, &lba) != 0) {
                puts("atapi: usage atapi read <0|1> <lba>\n");
                return;
            }
            int cls = IDE_CLASS_NONE;
            char m[40];
            if (ide_probe_type((uint8_t)dr, m, &cls) != 0 || cls != IDE_CLASS_ATAPI) {
                puts("atapi: drive is not ATAPI\n");
                return;
            }
            uint8_t sec[2048];
            if (ide_atapi_read2048((uint8_t)dr, (uint32_t)lba, sec) != 0) {
                puts("atapi: READ(10) failed (no media?)\n");
                return;
            }
            puts("READ lba=");
            puts_dec(lba);
            puts(" first 64 bytes:\n");
            const char *xd = "0123456789ABCDEF";
            for (int i = 0; i < 64; i++) {
                unsigned char v = sec[i];
                putchar(xd[v >> 4]);
                putchar(xd[v & 15]);
                putchar((i & 15) == 15 ? '\n' : ' ');
            }
        } else {
            puts("atapi: usage atapi inquiry <0|1> | atapi read <0|1> <lba>\n");
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
            if (chaseros_vol_attach_ide((uint8_t)d) != 0) {
                puts("attach: IDE failed (no disk or size too small)\n");
                return;
            }
            shell_cwd_reset();
            puts("attach: IDE volume ready");
            if (vfs_mount_root() == VFS_ERR_NONE) {
                puts(", VFS mounted\n");
            } else {
                puts("\n");
            }
        }
#ifdef CHASEROS_HAVE_AHCI_RUST
        else if (strncmp_local(p, "ahci", 4) == 0 && (p[4] == ' ' || p[4] == '\0')) {
            p += 4;
            skip_sp(&p);
            uint64_t ap = 0;
            if (parse_u64(p, &ap) != 0 || ap > 31u) {
                puts("attach: usage attach ahci <0-31>\n");
                return;
            }
            if (chaseros_vol_attach_ahci((uint32_t)ap) != 0) {
                puts("attach: AHCI failed (no ABAR, disk, or size too small)\n");
                return;
            }
            shell_cwd_reset();
            puts("attach: AHCI volume ready");
            if (vfs_mount_root() == VFS_ERR_NONE) {
                puts(", VFS mounted\n");
            } else {
                puts("\n");
            }
        }
#endif
        else {
#ifdef CHASEROS_HAVE_AHCI_RUST
            puts("attach: use 'attach ide <0|1>' or 'attach ahci <0-31>'\n");
#else
            puts("attach: only 'attach ide <0|1>' supported\n");
#endif
        }
    } else if (strcmp(verb, "lspci") == 0) {
        pci_list_devices();
    } else if (strcmp(verb, "ps") == 0) {
        sched_print_task_table();
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
        char cresolved[256];
        if (shell_resolve(name, cresolved, sizeof cresolved) != 0) {
            puts("cnrun: invalid path\n");
            return;
        }
        if (!vfs_is_mounted()) {
            puts("cnrun: VFS not mounted (try: mount)\n");
            return;
        }
        user_run_cnaf_from_path_streaming(cresolved);
    } else if (strcmp(verb, "slime") == 0) {
        user_run_embedded_slime_hello();
    } else if (strcmp(verb, "systeminfo") == 0) {
        sysinfo_print();
    } else if (strcmp(verb, "resolution") == 0) {
        shell_resolution_cmd(p);
    } else if (cmd[0] != '\0') {
        puts("Unknown command: ");
        puts(cmd);
        puts("\n");
    }
}

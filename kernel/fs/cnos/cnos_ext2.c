/* 与 libext2fs 对接的入口 */

#include "fs/cnos/porting.h"
#include "fs/version.h"
#include <stdint.h>

extern void puts(const char *);

void cnos_ext2_init(void) {
    puts("[FS] e2fsprogs ");
    puts(E2FSPROGS_VERSION);
    puts("\n");
}

/*
 * 若 buf 指向包含 ext2 超级块的标准布局（块大小 1024 时超块在 1024 字节处），
 * 则 s_magic 小端于 buf[0x438]（=1024+56）。
 */
int cnos_ext2_probe_super_le(const uint8_t *disk_base, uint32_t len) {
    const uint32_t off = 1024 + 56;
    if (len < off + 2) {
        return 0;
    }
    uint16_t magic = (uint16_t)disk_base[off] | ((uint16_t)disk_base[off + 1] << 8);
    return magic == 0xEF53;
}

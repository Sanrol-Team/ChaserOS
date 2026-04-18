/*
 * 单块组 ext2（rev 0），1024 字节块。
 * 元数据块 1–20，首数据块 21（根目录）、22（lost+found）。
 */

#include "fs/cnos/cnos_ext2_vol.h"
#include "drivers/ide.h"
#include "pmm.h"
#include "ext2_fs.h"
#include <stdint.h>
#include <stddef.h>

#define CNOS_BLK 1024u
#define INODE_TBL_BLOCKS 16u
/* 块 1..20 为超级块、GDT、位图、inode 表；块 21 起为数据 */
#define FIRST_DATA_BLOCK 21u

static cnos_vol_t g_vol;
static void *g_ram_alloc;
static uint64_t g_ram_npages;

extern void puts(const char *s);
extern void puts_dec(uint64_t n);

static void cnos_memset(void *d, int c, size_t n) {
    unsigned char *p = d;
    while (n--) {
        *p++ = (unsigned char)c;
    }
}

static void cnos_memcpy(void *d, const void *s, size_t n) {
    unsigned char *a = d;
    const unsigned char *b = s;
    while (n--) {
        *a++ = *b++;
    }
}

static size_t cnos_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static int cnos_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

void cnos_vol_init(void) {
    g_vol.type = CNOS_VOL_NONE;
    g_vol.ram_base = NULL;
    g_vol.size_bytes = 0;
    g_vol.ide_drive = 0;
    g_ram_alloc = NULL;
    g_ram_npages = 0;
}

const cnos_vol_t *cnos_vol_current(void) {
    return g_vol.type != CNOS_VOL_NONE ? &g_vol : NULL;
}

void cnos_vol_detach_ram(void) {
    if (g_ram_alloc && g_ram_npages) {
        pmm_free_contiguous(g_ram_alloc, g_ram_npages);
    }
    g_ram_alloc = NULL;
    g_ram_npages = 0;
    if (g_vol.type == CNOS_VOL_RAM) {
        g_vol.type = CNOS_VOL_NONE;
        g_vol.ram_base = NULL;
        g_vol.size_bytes = 0;
    }
}

void cnos_vol_detach_all(void) {
    cnos_vol_detach_ram();
    g_vol.type = CNOS_VOL_NONE;
    g_vol.ram_base = NULL;
    g_vol.size_bytes = 0;
}

int cnos_vol_attach_ram(void *base, size_t size_bytes) {
    if (!base || size_bytes < 256u * 1024u || (size_bytes % CNOS_BLK) != 0u) {
        return -1;
    }
    uint32_t nblk = (uint32_t)(size_bytes / CNOS_BLK);
    if (nblk < 256u || nblk > 8192u) {
        return -1;
    }
    cnos_vol_detach_all();
    g_vol.type = CNOS_VOL_RAM;
    g_vol.ram_base = (uint8_t *)base;
    g_vol.size_bytes = size_bytes;
    g_ram_alloc = base;
    g_ram_npages = (size_bytes + PAGE_SIZE - 1u) / PAGE_SIZE;
    return 0;
}

int cnos_vol_attach_ide(uint8_t drive) {
    uint32_t sec = 0;
    if (ide_capacity_sectors(drive, &sec) != 0) {
        return -1;
    }
    uint64_t bytes = ((uint64_t)sec * 512u) & ~1023ULL;
    if (bytes < 256u * 1024u) {
        return -1;
    }
    cnos_vol_detach_all();
    g_vol.type = CNOS_VOL_IDE;
    g_vol.ram_base = NULL;
    g_vol.size_bytes = (size_t)bytes;
    g_vol.ide_drive = drive;
    return 0;
}

static int vol_check(const cnos_vol_t *v) {
    if (!v || v->type == CNOS_VOL_NONE) {
        return -1;
    }
    if (v->size_bytes < 256u * 1024u || (v->size_bytes % CNOS_BLK) != 0u) {
        return -1;
    }
    return 0;
}

static int vol_read_blk(const cnos_vol_t *v, uint32_t blk, void *buf) {
    if (vol_check(v) != 0) {
        return -1;
    }
    if ((uint64_t)(blk + 1u) * CNOS_BLK > v->size_bytes) {
        return -1;
    }
    if (v->type == CNOS_VOL_RAM) {
        cnos_memcpy(buf, v->ram_base + (uint64_t)blk * CNOS_BLK, CNOS_BLK);
        return 0;
    }
    if (v->type == CNOS_VOL_IDE) {
        return ide_read_sectors(v->ide_drive, blk * 2u, 2u, buf);
    }
    return -1;
}

static int vol_write_blk(const cnos_vol_t *v, uint32_t blk, const void *buf) {
    if (vol_check(v) != 0) {
        return -1;
    }
    if ((uint64_t)(blk + 1u) * CNOS_BLK > v->size_bytes) {
        return -1;
    }
    if (v->type == CNOS_VOL_RAM) {
        cnos_memcpy(v->ram_base + (uint64_t)blk * CNOS_BLK, buf, CNOS_BLK);
        return 0;
    }
    if (v->type == CNOS_VOL_IDE) {
        return ide_write_sectors(v->ide_drive, blk * 2u, 2u, buf);
    }
    return -1;
}

static void bm_set(uint8_t *bm, uint32_t bit) {
    bm[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
}

static void bm_clr(uint8_t *bm, uint32_t bit) {
    bm[bit / 8u] &= (uint8_t)(~(1u << (bit % 8u)));
}

static int bm_get(const uint8_t *bm, uint32_t bit) {
    return (int)(bm[bit / 8u] >> (bit % 8u)) & 1;
}

static uint32_t total_blocks_from_vol(const cnos_vol_t *v) {
    return (uint32_t)(v->size_bytes / CNOS_BLK);
}

static int read_super(const cnos_vol_t *v, struct ext2_super_block *sb) {
    uint8_t blk[CNOS_BLK];
    if (vol_read_blk(v, 1u, blk) != 0) {
        return -1;
    }
    cnos_memcpy(sb, blk, sizeof(*sb));
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        return -1;
    }
    return 0;
}

static int write_super(const cnos_vol_t *v, const struct ext2_super_block *sb) {
    uint8_t blk[CNOS_BLK];
    cnos_memset(blk, 0, CNOS_BLK);
    cnos_memcpy(blk, sb, sizeof(*sb));
    return vol_write_blk(v, 1u, blk);
}

static int read_gd(const cnos_vol_t *v, struct ext2_group_desc *gd) {
    uint8_t blk[CNOS_BLK];
    if (vol_read_blk(v, 2u, blk) != 0) {
        return -1;
    }
    cnos_memcpy(gd, blk, sizeof(*gd));
    return 0;
}

static int write_gd(const cnos_vol_t *v, const struct ext2_group_desc *gd) {
    uint8_t blk[CNOS_BLK];
    cnos_memset(blk, 0, CNOS_BLK);
    cnos_memcpy(blk, gd, sizeof(*gd));
    return vol_write_blk(v, 2u, blk);
}

static void inode_to_raw(const struct ext2_inode *in, uint8_t *raw) {
    cnos_memcpy(raw, in, sizeof(struct ext2_inode));
}

static void raw_to_inode(const uint8_t *raw, struct ext2_inode *in) {
    cnos_memcpy(in, raw, sizeof(struct ext2_inode));
}

static int read_inode(const cnos_vol_t *v, uint32_t ino, struct ext2_inode *out) {
    if (ino == 0u) {
        return -1;
    }
    uint32_t idx = ino - 1u;
    uint32_t byte_off = idx * 128u;
    uint32_t blk = 5u + byte_off / CNOS_BLK;
    uint32_t off = byte_off % CNOS_BLK;
    uint8_t b[CNOS_BLK];
    if (vol_read_blk(v, blk, b) != 0) {
        return -1;
    }
    raw_to_inode(b + off, out);
    return 0;
}

static int write_inode(const cnos_vol_t *v, uint32_t ino, const struct ext2_inode *in) {
    uint32_t idx = ino - 1u;
    uint32_t byte_off = idx * 128u;
    uint32_t blk = 5u + byte_off / CNOS_BLK;
    uint32_t off = byte_off % CNOS_BLK;
    uint8_t b[CNOS_BLK];
    if (vol_read_blk(v, blk, b) != 0) {
        return -1;
    }
    uint8_t raw[128];
    inode_to_raw(in, raw);
    cnos_memcpy(b + off, raw, 128u);
    return vol_write_blk(v, blk, b);
}

static void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void dirent_put(uint8_t *base, uint32_t *off, uint32_t ino, const char *name) {
    size_t nl = cnos_strlen(name);
    uint16_t name_len = (uint16_t)nl;
    uint16_t rec = (uint16_t)(8u + name_len);
    rec = (uint16_t)((rec + 3u) & ~3u);
    put_le32(base + *off, ino);
    put_le16(base + *off + 4u, rec);
    put_le16(base + *off + 6u, name_len);
    cnos_memcpy(base + *off + 8u, name, nl);
    *off += rec;
}

static uint32_t find_in_root(const uint8_t *blk, const char *name, uint32_t *out_ino) {
    uint32_t pos = 0u;
    while (pos + 8u <= CNOS_BLK) {
        uint32_t ino = (uint32_t)blk[pos] | ((uint32_t)blk[pos + 1u] << 8u) |
                       ((uint32_t)blk[pos + 2u] << 16u) | ((uint32_t)blk[pos + 3u] << 24u);
        uint16_t rec = (uint16_t)(blk[pos + 4u] | (blk[pos + 5u] << 8));
        uint16_t nl = (uint16_t)((blk[pos + 6u] | (blk[pos + 7u] << 8)) & 0xFFu);
        if (rec < 8u) {
            break;
        }
        char nm[256];
        if (nl >= sizeof nm) {
            return 0u;
        }
        cnos_memcpy(nm, blk + pos + 8u, nl);
        nm[nl] = '\0';
        if (cnos_strcmp(nm, name) == 0) {
            if (out_ino) {
                *out_ino = ino;
            }
            return 1u;
        }
        (void)ino;
        pos += rec;
    }
    return 0u;
}

int cnos_ext2_format(const cnos_vol_t *v) {
    if (vol_check(v) != 0) {
        return -1;
    }
    uint32_t tb = total_blocks_from_vol(v);
    if (tb < 256u || tb > 8192u || tb < FIRST_DATA_BLOCK + 2u) {
        return -1;
    }

    if (v->type == CNOS_VOL_RAM) {
        cnos_memset(v->ram_base, 0, v->size_bytes);
    }

    const uint32_t used_blocks_mark = FIRST_DATA_BLOCK + 2u; /* 0..22 */
    const uint32_t used_inodes_mark = 11u;

    uint32_t root_data = FIRST_DATA_BLOCK;
    uint32_t lf_data = FIRST_DATA_BLOCK + 1u;

    struct ext2_super_block sb;
    cnos_memset(&sb, 0, sizeof sb);
    sb.s_inodes_count = 128u;
    sb.s_blocks_count = tb;
    sb.s_r_blocks_count = 8u;
    sb.s_free_blocks_count = tb - used_blocks_mark;
    sb.s_free_inodes_count = 128u - used_inodes_mark;
    sb.s_first_data_block = 1u;
    sb.s_log_block_size = 0u;
    sb.s_log_cluster_size = 0u;
    sb.s_blocks_per_group = tb;
    sb.s_clusters_per_group = tb;
    sb.s_inodes_per_group = 128u;
    sb.s_magic = EXT2_SUPER_MAGIC;
    sb.s_state = 1u;
    sb.s_errors = 1u;
    sb.s_rev_level = EXT2_GOOD_OLD_REV;
    sb.s_creator_os = EXT2_OS_LINUX;

    struct ext2_group_desc gd;
    cnos_memset(&gd, 0, sizeof gd);
    gd.bg_block_bitmap = 3u;
    gd.bg_inode_bitmap = 4u;
    gd.bg_inode_table = 5u;
    gd.bg_free_blocks_count =
        (uint16_t)(sb.s_free_blocks_count > 0xFFFFu ? 0xFFFFu : sb.s_free_blocks_count);
    gd.bg_free_inodes_count =
        (uint16_t)(sb.s_free_inodes_count > 0xFFFFu ? 0xFFFFu : sb.s_free_inodes_count);
    gd.bg_used_dirs_count = 2u;

    uint8_t bb[CNOS_BLK];
    uint8_t ib[CNOS_BLK];
    cnos_memset(bb, 0, CNOS_BLK);
    cnos_memset(ib, 0, CNOS_BLK);
    uint32_t b;
    for (b = 0u; b < used_blocks_mark; b++) {
        bm_set(bb, b);
    }
    for (b = 0u; b < used_inodes_mark; b++) {
        bm_set(ib, b);
    }

    struct ext2_inode in_root;
    cnos_memset(&in_root, 0, sizeof in_root);
    in_root.i_mode = (uint16_t)(0x4000u | 0755u);
    in_root.i_size = CNOS_BLK;
    in_root.i_links_count = 3u;
    in_root.i_blocks = 2u;
    in_root.i_block[0] = root_data;

    struct ext2_inode in_lf;
    cnos_memset(&in_lf, 0, sizeof in_lf);
    in_lf.i_mode = (uint16_t)(0x4000u | 0755u);
    in_lf.i_size = CNOS_BLK;
    in_lf.i_links_count = 2u;
    in_lf.i_blocks = 2u;
    in_lf.i_block[0] = lf_data;

    uint8_t droot[CNOS_BLK];
    cnos_memset(droot, 0, CNOS_BLK);
    uint32_t doff = 0u;
    dirent_put(droot, &doff, 2u, ".");
    dirent_put(droot, &doff, 2u, "..");
    dirent_put(droot, &doff, 11u, "lost+found");

    uint8_t dlf[CNOS_BLK];
    cnos_memset(dlf, 0, CNOS_BLK);
    doff = 0u;
    dirent_put(dlf, &doff, 11u, ".");
    dirent_put(dlf, &doff, 2u, "..");

    if (write_super(v, &sb) != 0 || write_gd(v, &gd) != 0) {
        return -1;
    }
    if (vol_write_blk(v, 3u, bb) != 0 || vol_write_blk(v, 4u, ib) != 0) {
        return -1;
    }

    uint32_t iblk;
    uint8_t zblk[CNOS_BLK];
    cnos_memset(zblk, 0, CNOS_BLK);
    for (iblk = 5u; iblk < 5u + INODE_TBL_BLOCKS; iblk++) {
        if (vol_write_blk(v, iblk, zblk) != 0) {
            return -1;
        }
    }
    if (write_inode(v, 2u, &in_root) != 0 || write_inode(v, 11u, &in_lf) != 0) {
        return -1;
    }

    if (vol_write_blk(v, root_data, droot) != 0 || vol_write_blk(v, lf_data, dlf) != 0) {
        return -1;
    }
    return 0;
}

int cnos_ext2_ls(const cnos_vol_t *v) {
    struct ext2_super_block sb;
    if (read_super(v, &sb) != 0) {
        puts("ls: not ext2 or unreadable superblock\n");
        return -1;
    }
    (void)sb;
    struct ext2_inode ir;
    if (read_inode(v, 2u, &ir) != 0) {
        return -1;
    }
    uint32_t db = ir.i_block[0];
    uint8_t blk[CNOS_BLK];
    if (vol_read_blk(v, db, blk) != 0) {
        return -1;
    }
    uint32_t pos = 0u;
    while (pos + 8u <= CNOS_BLK) {
        uint32_t ino = (uint32_t)blk[pos] | ((uint32_t)blk[pos + 1u] << 8u) |
                       ((uint32_t)blk[pos + 2u] << 16u) | ((uint32_t)blk[pos + 3u] << 24u);
        uint16_t rec = (uint16_t)(blk[pos + 4u] | (blk[pos + 5u] << 8));
        uint16_t nl = (uint16_t)((blk[pos + 6u] | (blk[pos + 7u] << 8)) & 0xFFu);
        if (rec < 8u) {
            break;
        }
        char name[256];
        if (nl >= sizeof name) {
            return -1;
        }
        cnos_memcpy(name, blk + pos + 8u, nl);
        name[nl] = '\0';
        puts("  ");
        puts(name);
        puts("  ino=");
        puts_dec((uint64_t)ino);
        puts("\n");
        pos += rec;
    }
    return 0;
}

int cnos_ext2_read_file(const cnos_vol_t *v, const char *name, char *buf, size_t buf_sz,
                        size_t *out_len) {
    if (!name || !buf || !out_len || buf_sz == 0u) {
        return -1;
    }
    struct ext2_super_block sb;
    if (read_super(v, &sb) != 0) {
        return -1;
    }
    (void)sb;
    struct ext2_inode ir;
    if (read_inode(v, 2u, &ir) != 0) {
        return -1;
    }
    uint8_t dblk[CNOS_BLK];
    if (vol_read_blk(v, ir.i_block[0], dblk) != 0) {
        return -1;
    }
    uint32_t fino = 0u;
    if (!find_in_root(dblk, name, &fino) || fino == 0u) {
        return -1;
    }
    struct ext2_inode fi;
    if (read_inode(v, fino, &fi) != 0) {
        return -1;
    }
    if ((fi.i_mode & 0xF000u) != 0x8000u) {
        return -1;
    }
    uint32_t maxread = fi.i_size;
    if (maxread > buf_sz) {
        maxread = (uint32_t)buf_sz;
    }
    uint32_t nblk = (fi.i_size + CNOS_BLK - 1u) / CNOS_BLK;
    if (nblk > 12u) {
        return -1;
    }
    uint32_t i;
    uint32_t off = 0u;
    for (i = 0u; i < nblk && off < maxread; i++) {
        uint32_t bn = fi.i_block[i];
        if (bn == 0u) {
            break;
        }
        uint8_t tmp[CNOS_BLK];
        if (vol_read_blk(v, bn, tmp) != 0) {
            return -1;
        }
        uint32_t chunk = CNOS_BLK;
        if (off + chunk > fi.i_size) {
            chunk = fi.i_size - off;
        }
        if (off + chunk > maxread) {
            chunk = maxread - off;
        }
        cnos_memcpy(buf + off, tmp, chunk);
        off += chunk;
    }
    *out_len = (size_t)off;
    return 0;
}

static uint32_t alloc_block(uint8_t *bb, uint32_t tb) {
    uint32_t bit;
    for (bit = FIRST_DATA_BLOCK + 2u; bit < tb; bit++) {
        if (!bm_get(bb, bit)) {
            bm_set(bb, bit);
            return bit;
        }
    }
    return 0u;
}

static uint32_t alloc_inode(uint8_t *ib) {
    uint32_t ino;
    for (ino = 12u; ino <= 128u; ino++) {
        if (!bm_get(ib, ino - 1u)) {
            bm_set(ib, ino - 1u);
            return ino;
        }
    }
    return 0u;
}

static void free_inode_blocks(const cnos_vol_t *v, struct ext2_super_block *sb,
                              struct ext2_group_desc *gd, uint8_t *bb, struct ext2_inode *fi) {
    uint32_t nblk = (fi->i_size + CNOS_BLK - 1u) / CNOS_BLK;
    if (nblk > 12u) {
        nblk = 12u;
    }
    uint32_t i;
    for (i = 0u; i < nblk; i++) {
        uint32_t bn = fi->i_block[i];
        if (bn != 0u) {
            bm_clr(bb, bn);
            sb->s_free_blocks_count++;
            gd->bg_free_blocks_count++;
        }
        fi->i_block[i] = 0u;
    }
    fi->i_size = 0u;
    fi->i_blocks = 0u;
}

int cnos_ext2_write_file(const cnos_vol_t *v, const char *name, const char *data, size_t len) {
    if (!name || (len > 0 && !data)) {
        return -1;
    }
    if (len > 12u * CNOS_BLK) {
        return -1;
    }

    struct ext2_super_block sb;
    struct ext2_group_desc gd;
    uint8_t bb[CNOS_BLK];
    uint8_t ib[CNOS_BLK];
    if (read_super(v, &sb) != 0 || read_gd(v, &gd) != 0) {
        return -1;
    }
    if (vol_read_blk(v, 3u, bb) != 0 || vol_read_blk(v, 4u, ib) != 0) {
        return -1;
    }

    uint32_t tb = total_blocks_from_vol(v);
    struct ext2_inode root_ino;
    if (read_inode(v, 2u, &root_ino) != 0) {
        return -1;
    }
    uint8_t droot[CNOS_BLK];
    if (vol_read_blk(v, root_ino.i_block[0], droot) != 0) {
        return -1;
    }

    uint32_t exist_ino = 0u;
    int exists = (int)find_in_root(droot, name, &exist_ino);

    uint32_t nblk = (uint32_t)((len + CNOS_BLK - 1u) / CNOS_BLK);
    if (nblk > 12u) {
        return -1;
    }

    struct ext2_inode fi;
    cnos_memset(&fi, 0, sizeof fi);

    if (exists && exist_ino != 0u) {
        if (read_inode(v, exist_ino, &fi) != 0) {
            return -1;
        }
        free_inode_blocks(v, &sb, &gd, bb, &fi);
        if (vol_write_blk(v, 3u, bb) != 0) {
            return -1;
        }
    } else {
        uint32_t new_ino = alloc_inode(ib);
        if (new_ino == 0u) {
            return -1;
        }
        exist_ino = new_ino;
        sb.s_free_inodes_count--;
        gd.bg_free_inodes_count--;
        uint32_t pos = 0u;
        while (pos + 8u <= CNOS_BLK) {
            uint16_t rec = (uint16_t)(droot[pos + 4u] | (droot[pos + 5u] << 8));
            if (rec < 8u) {
                break;
            }
            pos += rec;
        }
        uint16_t nl = (uint16_t)cnos_strlen(name);
        uint16_t need = (uint16_t)((8u + nl + 3u) & ~3u);
        if (pos + need > CNOS_BLK) {
            return -1;
        }
        dirent_put(droot, &pos, exist_ino, name);
        if (vol_write_blk(v, root_ino.i_block[0], droot) != 0) {
            return -1;
        }
    }

    fi.i_mode = (uint16_t)(0x8000u | 0644u);
    fi.i_size = (uint32_t)len;
    fi.i_links_count = 1u;
    fi.i_blocks = (uint32_t)(((len + 511u) / 512u) & 0xFFFFFFFFu);
    if (fi.i_blocks == 0u && len > 0u) {
        fi.i_blocks = 1u;
    }

    uint32_t bi;
    for (bi = 0u; bi < nblk; bi++) {
        uint32_t bn = alloc_block(bb, tb);
        if (bn == 0u) {
            return -1;
        }
        sb.s_free_blocks_count--;
        gd.bg_free_blocks_count--;
        fi.i_block[bi] = bn;
        uint8_t tmp[CNOS_BLK];
        cnos_memset(tmp, 0, CNOS_BLK);
        uint32_t chunk = CNOS_BLK;
        if (bi * CNOS_BLK + chunk > len) {
            chunk = (uint32_t)(len - bi * CNOS_BLK);
        }
        cnos_memcpy(tmp, data + bi * CNOS_BLK, chunk);
        if (vol_write_blk(v, bn, tmp) != 0) {
            return -1;
        }
    }

    if (write_super(v, &sb) != 0 || write_gd(v, &gd) != 0 || vol_write_blk(v, 3u, bb) != 0 ||
        vol_write_blk(v, 4u, ib) != 0) {
        return -1;
    }
    if (write_inode(v, exist_ino, &fi) != 0) {
        return -1;
    }
    return 0;
}

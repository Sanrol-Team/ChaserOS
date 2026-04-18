/* kernel/power.c - ACPI 关机 / 重启
 *
 * 常见失效原因（含 VMware）：
 * - 固定 SLP_TYP：须从 DSDT 的 \_S5_ 取休眠编码。
 * - HW_REDUCED_ACPI：无 PM1，应写 FADT.sleep_control（ACPI 5+）。
 * - PM1a 端口为 0 时须用 xpm1a_control_block（GAS）。
 */

#include "power.h"
#include "io.h"
#include <stddef.h>
#include <stdint.h>

struct acpi_table_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_gas {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

#define ACPI_FADT_HW_REDUCED (1u << 20)

/* Linux actbl.h 对齐的 FADT 字段偏移 */
#define FADT_DSDT           0x28u
#define FADT_SMI_CMD        0x30u
#define FADT_ACPI_ENABLE    0x34u
#define FADT_PM1A_CNT_BLK   0x40u
#define FADT_PM1B_CNT_BLK   0x44u
#define FADT_FLAGS          0x70u
#define FADT_RESET_REG      0x74u
#define FADT_RESET_VALUE    0x80u
#define FADT_XDSDT          0x8Cu
#define FADT_XPM1A_CNT      0xACu
#define FADT_SLEEP_CONTROL  0xF4u

/* ACPI 5.0+ Sleep Control 寄存器（HW-reduced） */
#define ACPI_X_SLEEP_ENABLE 0x20u

#define PM1_CNT_SCI_EN 0x0001u
#define PM1_CNT_SLP_EN 0x2000u

static uint8_t acpi_sum_bytes(const uint8_t *p, size_t n) {
    uint8_t s = 0;
    for (size_t i = 0; i < n; i++) {
        s = (uint8_t)(s + p[i]);
    }
    return s;
}

static int rsdp_checksum_ok(const uint8_t *p, size_t len) {
    return acpi_sum_bytes(p, len) == 0;
}

static const uint8_t *find_rsdp(void) {
    for (uintptr_t a = 0xE0000; a < 0x100000; a += 16) {
        const uint8_t *p = (const uint8_t *)a;
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' && p[4] == 'P' && p[5] == 'T' &&
            p[6] == 'R' && p[7] == ' ') {
            size_t len = p[15] >= 2 ? 36u : 20u;
            if (rsdp_checksum_ok(p, len)) {
                return p;
            }
        }
    }
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    volatile uint16_t *ebda_ptr = (volatile uint16_t *)(uintptr_t)0x40E;
    uint16_t ebda_seg = *ebda_ptr;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    uintptr_t ebda = (uintptr_t)ebda_seg * 16u;
    if (ebda >= 0x1000u && ebda < 0xA0000u) {
        for (uintptr_t a = ebda; a < ebda + 1024; a += 16) {
            const uint8_t *p = (const uint8_t *)a;
            if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' && p[4] == 'P' && p[5] == 'T' &&
                p[6] == 'R' && p[7] == ' ') {
                size_t len = p[15] >= 2 ? 36u : 20u;
                if (rsdp_checksum_ok(p, len)) {
                    return p;
                }
            }
        }
    }
    return NULL;
}

static int header_sig_eq(const struct acpi_table_header *h, char a, char b, char c, char d) {
    return h->signature[0] == a && h->signature[1] == b && h->signature[2] == c && h->signature[3] == d;
}

static int acpi_phys_ok64(uint64_t p) {
    return p != 0 && p <= 0xFFFFFFFFull;
}

static int acpi_phys_ok32(uint32_t p) {
    return p != 0;
}

static const uint8_t *find_facp(void) {
    const uint8_t *rsdp = find_rsdp();
    if (!rsdp) {
        return NULL;
    }
    uint8_t rev = rsdp[15];
    uint32_t rsdt_phys = *(const uint32_t *)(rsdp + 16);
    uint64_t xsdt_phys = 0;
    if (rev >= 2) {
        xsdt_phys = *(const uint64_t *)(rsdp + 24);
    }

    if (acpi_phys_ok64(xsdt_phys)) {
        const struct acpi_table_header *xsdt = (const struct acpi_table_header *)(uintptr_t)xsdt_phys;
        if (header_sig_eq(xsdt, 'X', 'S', 'D', 'T') && xsdt->length >= sizeof(struct acpi_table_header) &&
            acpi_sum_bytes((const uint8_t *)xsdt, xsdt->length) == 0) {
            uint32_t n = (xsdt->length - (uint32_t)sizeof(struct acpi_table_header)) / 8u;
            const uint64_t *entries = (const uint64_t *)((const uint8_t *)xsdt + sizeof(struct acpi_table_header));
            for (uint32_t i = 0; i < n; i++) {
                uint64_t phys = entries[i];
                if (!acpi_phys_ok64(phys)) {
                    continue;
                }
                const struct acpi_table_header *h = (const struct acpi_table_header *)(uintptr_t)phys;
                if (header_sig_eq(h, 'F', 'A', 'C', 'P') && h->length >= 128u &&
                    acpi_sum_bytes((const uint8_t *)h, h->length) == 0) {
                    return (const uint8_t *)h;
                }
            }
        }
    }

    if (acpi_phys_ok32(rsdt_phys)) {
        const struct acpi_table_header *rsdt = (const struct acpi_table_header *)(uintptr_t)rsdt_phys;
        if (header_sig_eq(rsdt, 'R', 'S', 'D', 'T') && rsdt->length >= sizeof(struct acpi_table_header) &&
            acpi_sum_bytes((const uint8_t *)rsdt, rsdt->length) == 0) {
            uint32_t n = (rsdt->length - (uint32_t)sizeof(struct acpi_table_header)) / 4u;
            const uint32_t *entries = (const uint32_t *)((const uint8_t *)rsdt + sizeof(struct acpi_table_header));
            for (uint32_t i = 0; i < n; i++) {
                uint32_t phys = entries[i];
                if (!acpi_phys_ok32(phys)) {
                    continue;
                }
                const struct acpi_table_header *h = (const struct acpi_table_header *)(uintptr_t)phys;
                if (header_sig_eq(h, 'F', 'A', 'C', 'P') && h->length >= 128u &&
                    acpi_sum_bytes((const uint8_t *)h, h->length) == 0) {
                    return (const uint8_t *)h;
                }
            }
        }
    }
    return NULL;
}

static uint32_t fadt_get_dsdt_phys(const uint8_t *fadt, uint32_t facp_len) {
    uint32_t dsdt = *(const uint32_t *)(fadt + FADT_DSDT);
    if (dsdt != 0) {
        return dsdt;
    }
    if (facp_len >= FADT_XDSDT + 8u) {
        uint64_t x = *(const uint64_t *)(fadt + FADT_XDSDT);
        if (acpi_phys_ok64(x)) {
            return (uint32_t)x;
        }
    }
    return 0;
}

/* Name(\_S5_, Package(){typ,typ}) 内常见字节序列：… 0A typa 0A typb（BytePrefix） */
static int parse_s5_sleep_typ(const uint8_t *aml, uint32_t len, uint8_t *typa, uint8_t *typb) {
    for (uint32_t i = 0; i + 10 < len; i++) {
        if (aml[i] != 0x08u || aml[i + 1] != 0x5Fu || aml[i + 2] != 0x53u || aml[i + 3] != 0x35u || aml[i + 4] != 0x5Fu) {
            continue;
        }
        uint32_t end = i + 5u + 64u;
        if (end > len) {
            end = len;
        }
        for (uint32_t k = i + 5u; k + 3u < end; k++) {
            if (aml[k] == 0x0Au && aml[k + 2u] == 0x0Au) {
                *typa = aml[k + 1u];
                *typb = aml[k + 3u];
                return 0;
            }
        }
    }
    return -1;
}

static void load_s5_typ(const uint8_t *fadt, uint32_t facp_len, uint8_t *typa, uint8_t *typb) {
    *typa = 5;
    *typb = 5;
    uint32_t dsp = fadt_get_dsdt_phys(fadt, facp_len);
    if (!acpi_phys_ok32(dsp)) {
        return;
    }
    const struct acpi_table_header *dh = (const struct acpi_table_header *)(uintptr_t)dsp;
    if (!header_sig_eq(dh, 'D', 'S', 'D', 'T') || dh->length < sizeof(struct acpi_table_header)) {
        return;
    }
    if (acpi_sum_bytes((const uint8_t *)dh, dh->length) != 0) {
        return;
    }
    uint32_t aml_len = dh->length - (uint32_t)sizeof(struct acpi_table_header);
    const uint8_t *aml = (const uint8_t *)dh + sizeof(struct acpi_table_header);
    uint8_t a = *typa, b = *typb;
    if (parse_s5_sleep_typ(aml, aml_len, &a, &b) == 0) {
        *typa = a;
        *typb = b;
    }
}

static void gas_write8(const struct acpi_gas *gas, uint8_t val) {
    if (gas->address_space_id == 1u && gas->address <= 0xFFFFu) {
        outb((uint16_t)gas->address, val);
        io_wait();
    } else if (gas->address_space_id == 0u && acpi_phys_ok64(gas->address)) {
        volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)gas->address;
        *p = val;
        __asm__ volatile("" ::: "memory");
    }
}

static void shutdown_hw_reduced(const uint8_t *fadt, uint32_t facp_len, uint8_t typa) {
    if (facp_len < FADT_SLEEP_CONTROL + sizeof(struct acpi_gas)) {
        return;
    }
    const struct acpi_gas *sc = (const struct acpi_gas *)(fadt + FADT_SLEEP_CONTROL);
    if (sc->address == 0) {
        return;
    }
    uint8_t v = (uint8_t)(((typa & 7u) << 2) | ACPI_X_SLEEP_ENABLE);
    gas_write8(sc, v);
}

static void shutdown_legacy_pm1(const uint8_t *fadt, uint32_t facp_len, uint8_t typa, uint8_t typb) {
    uint32_t smi_cmd = *(const uint32_t *)(fadt + FADT_SMI_CMD);
    uint8_t acpi_en = *(const uint8_t *)(fadt + FADT_ACPI_ENABLE);
    uint32_t pm1a_phys = *(const uint32_t *)(fadt + FADT_PM1A_CNT_BLK);
    uint32_t pm1b_phys = *(const uint32_t *)(fadt + FADT_PM1B_CNT_BLK);
    const struct acpi_gas *xpm1a = NULL;
    const struct acpi_gas *xpm1b = NULL;
    if (facp_len >= FADT_XPM1A_CNT + 2u * sizeof(struct acpi_gas)) {
        xpm1a = (const struct acpi_gas *)(fadt + FADT_XPM1A_CNT);
        xpm1b = (const struct acpi_gas *)(fadt + FADT_XPM1A_CNT + sizeof(struct acpi_gas));
    }

    int have_pm1a = (pm1a_phys != 0) || (xpm1a && xpm1a->address != 0);
    if (!have_pm1a) {
        return;
    }

    if (smi_cmd != 0 && acpi_en != 0) {
        outb((uint16_t)smi_cmd, acpi_en);
        for (int w = 0; w < 500; w++) {
            io_wait();
        }
    }

    if (pm1a_phys != 0) {
        uint16_t pm1a = (uint16_t)pm1a_phys;
        uint16_t cur = inw(pm1a);
        if (!(cur & PM1_CNT_SCI_EN)) {
            for (int t = 0; t < 1000; t++) {
                cur = inw(pm1a);
                if (cur & PM1_CNT_SCI_EN) {
                    break;
                }
                io_wait();
            }
        }
        cur = inw(pm1a);
        uint16_t sleep = (uint16_t)((cur & 0xE3FFu) | (uint16_t)((typa & 7u) << 10) | PM1_CNT_SLP_EN);
        outw(pm1a, sleep);
    } else if (xpm1a && xpm1a->address != 0) {
        if (xpm1a->address_space_id == 1u && xpm1a->address <= 0xFFFFu) {
            uint16_t pm = (uint16_t)xpm1a->address;
            uint16_t cur = inw(pm);
            uint16_t sleep = (uint16_t)((cur & 0xE3FFu) | (uint16_t)((typa & 7u) << 10) | PM1_CNT_SLP_EN);
            outw(pm, sleep);
        } else if (xpm1a->address_space_id == 0u && acpi_phys_ok64(xpm1a->address)) {
            volatile uint16_t *mp = (volatile uint16_t *)(uintptr_t)xpm1a->address;
            uint16_t cur = *mp;
            *mp = (uint16_t)((cur & 0xE3FFu) | (uint16_t)((typa & 7u) << 10) | PM1_CNT_SLP_EN);
        }
    }

    if (pm1b_phys != 0) {
        uint16_t pm1b = (uint16_t)pm1b_phys;
        uint16_t bcur = inw(pm1b);
        uint16_t bsleep = (uint16_t)((bcur & 0xE3FFu) | (uint16_t)((typb & 7u) << 10) | PM1_CNT_SLP_EN);
        outw(pm1b, bsleep);
    } else if (xpm1b && xpm1b->address != 0 && xpm1b->address_space_id == 1u && xpm1b->address <= 0xFFFFu) {
        uint16_t pm = (uint16_t)xpm1b->address;
        uint16_t bcur = inw(pm);
        uint16_t bsleep = (uint16_t)((bcur & 0xE3FFu) | (uint16_t)((typb & 7u) << 10) | PM1_CNT_SLP_EN);
        outw(pm, bsleep);
    }
}

static void shutdown_emulator_ports(void) {
    outw(0x604, 0x2000);
    io_wait();
    outw(0xB004, 0x2000);
    io_wait();
}

void power_shutdown(void) {
    __asm__ volatile("cli");

    const uint8_t *fadt = find_facp();
    if (fadt) {
        uint32_t facp_len = *(const uint32_t *)(fadt + 4);
        uint8_t typa = 5, typb = 5;
        load_s5_typ(fadt, facp_len, &typa, &typb);

        uint32_t flags = *(const uint32_t *)(fadt + FADT_FLAGS);
        int did_hwr = 0;
        if ((flags & ACPI_FADT_HW_REDUCED) != 0 && facp_len >= FADT_SLEEP_CONTROL + sizeof(struct acpi_gas)) {
            const struct acpi_gas *sc = (const struct acpi_gas *)(fadt + FADT_SLEEP_CONTROL);
            if (sc->address != 0) {
                shutdown_hw_reduced(fadt, facp_len, typa);
                did_hwr = 1;
            }
        }
        if (!did_hwr) {
            shutdown_legacy_pm1(fadt, facp_len, typa, typb);
        }
    }
    shutdown_emulator_ports();

    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void gas_write_reset(const struct acpi_gas *gas, uint8_t val) {
    gas_write8(gas, val);
}

static void reboot_acpi_reset_reg(const uint8_t *fadt, uint32_t facp_len) {
    if (fadt == NULL || facp_len < 0x81u) {
        return;
    }
    const struct acpi_gas *gas = (const struct acpi_gas *)(fadt + FADT_RESET_REG);
    uint8_t reset_val = *(const uint8_t *)(fadt + FADT_RESET_VALUE);
    if (gas->address == 0) {
        return;
    }
    gas_write_reset(gas, reset_val);
}

static void reboot_ich_reset_port(void) {
    uint8_t x = inb(0xCF9);
    outb(0xCF9, (uint8_t)((x & (uint8_t)~0x02u) | 0x02u));
    io_wait();
    outb(0xCF9, 0x06u);
    io_wait();
}

static void reboot_ps2_pulse(void) {
    uint8_t good = 0x02;
    int spins = 0;
    while ((good & 0x02) && spins < 100000) {
        good = inb(0x64);
        spins++;
    }
    outb(0x64, 0xFE);
}

static void reboot_triple_fault(void) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile("lidt %0" :: "m"(null_idt));
    __asm__ volatile("ud2");
}

void power_reboot(void) {
    __asm__ volatile("cli");

    const uint8_t *fadt = find_facp();
    uint32_t facp_len = 0;
    if (fadt) {
        facp_len = *(const uint32_t *)(fadt + 4);
    }

    /* ICH 在 VMware/实体机常见；先试硬复位再 ACPI */
    reboot_ich_reset_port();
    reboot_acpi_reset_reg(fadt, facp_len);
    reboot_ps2_pulse();
    reboot_triple_fault();

    for (;;) {
        __asm__ volatile("hlt");
    }
}

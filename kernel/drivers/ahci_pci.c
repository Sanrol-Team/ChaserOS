/* PCI 枚举 AHCI（class 0x010601），启用 MMIO+BusMaster，调用 Rust AHCI */

#include "drivers/ahci_rust_api.h"
#include "drivers/pci.h"
#include <stddef.h>
#include <stdint.h>

extern void puts(const char *s);
extern void puts_hex(uint64_t n);

/** 最近一次探测成功的 ABAR（identity 映射下的 MMIO 指针）；未探测或失败时为 NULL */
static uint32_t *g_ahci_abar_mmio;

uint32_t *chaseros_ahci_get_abar_mmio(void) {
    return g_ahci_abar_mmio;
}

static int find_ahci_bar5(uint64_t *abar_phys_out) {
    if (!abar_phys_out) {
        return -1;
    }
    for (uint16_t bus = 0; bus < 256u; bus++) {
        for (uint8_t dev = 0; dev < 32u; dev++) {
            for (uint8_t func = 0; func < 8u; func++) {
                uint32_t id = pci_read_config((uint8_t)bus, dev, func, 0);
                if ((id & 0xFFFFu) == 0xFFFFu) {
                    continue;
                }
                uint32_t cls = pci_read_config((uint8_t)bus, dev, func, 8);
                if (((cls >> 8) & 0xFFFFFFu) != 0x010601u) {
                    continue;
                }
                uint32_t bar5 = pci_read_config((uint8_t)bus, dev, func, 0x24);
                uint64_t abar_phys;
                if ((bar5 & 1u) != 0u) {
                    /* I/O BAR（AHCI ABAR 一般为 MMIO，此项少见） */
                    abar_phys = (uint64_t)(bar5 & 0xFFFFFFFCu);
                } else if ((bar5 & 6u) == 4u) {
                    /* 64-bit MMIO：低 32 位 + BAR6 高 32 位 */
                    uint32_t hi = pci_read_config((uint8_t)bus, dev, func, 0x28);
                    abar_phys =
                        ((uint64_t)hi << 32u) | ((uint64_t)(bar5 & 0xFFFFFFF0u));
                } else {
                    /* 典型情况：32-bit memory BAR */
                    abar_phys = (uint64_t)(bar5 & 0xFFFFFFF0u);
                }
                if (abar_phys == 0u) {
                    continue;
                }
                uint32_t cmd = pci_read_config((uint8_t)bus, dev, func, 4);
                pci_write_config((uint8_t)bus, dev, func, 4, cmd | 7u);
                *abar_phys_out = abar_phys;
                return 0;
            }
        }
    }
    return -1;
}

void chaseros_ahci_pci_probe(void) {
    g_ahci_abar_mmio = NULL;
    uint64_t abar = 0;
    if (find_ahci_bar5(&abar) != 0) {
        puts("[AHCI] no SATA AHCI controller (PCI class 01:06:01)\n");
        return;
    }
    puts("[AHCI] ABAR=");
    puts_hex(abar);
    uint32_t *mmio = (uint32_t *)(uintptr_t)abar;
    g_ahci_abar_mmio = mmio;
    uint32_t pi = chaseros_ahci_rust_init(mmio);
    puts(" PI=");
    puts_hex((uint64_t)pi);
    puts("\n");

    uint8_t id[512];
    if (chaseros_ahci_rust_identify(mmio, 0u, id) == 0) {
        puts("[AHCI] port0 IDENTIFY DMA ok (toy driver)\n");
    } else {
        puts("[AHCI] port0 IDENTIFY failed (expected if no disk on AHCI)\n");
    }
}

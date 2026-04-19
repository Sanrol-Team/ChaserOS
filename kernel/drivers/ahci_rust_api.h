/* Rust staticlib kernel/ahci_rust — C ABI */

#ifndef AHCI_RUST_API_H
#define AHCI_RUST_API_H

#include <stdint.h>

uint32_t chaseros_ahci_rust_init(uint32_t *abar_mmio);
int chaseros_ahci_rust_identify(uint32_t *abar_mmio, uint32_t port, uint8_t *out512);
int chaseros_ahci_rust_capacity_sectors(uint32_t *abar_mmio, uint32_t port, uint32_t *out_sec);
int chaseros_ahci_rust_rw_sectors(uint32_t *abar_mmio, uint32_t port, uint32_t lba, uint32_t nsect,
                                  uint8_t *buf, int is_write);

#endif

/* kernel/drivers/pci.h - PCI 总线驱动 */

#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

void pci_init(void);
void pci_list_devices(void);
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

#endif

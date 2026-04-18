/* kernel/drivers/pci.h - PCI 总线驱动 */

#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

void pci_init();
void pci_list_devices();

#endif

#ifndef AHCI_PCI_H
#define AHCI_PCI_H

#include <stdint.h>

void chaseros_ahci_pci_probe(void);
uint32_t *chaseros_ahci_get_abar_mmio(void);

#endif

/* Primary IDE / ATA PIO (LBA28), 轮询方式，无需 IRQ */

#include "drivers/ide.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

#define ATA_REG_DATA       0x00u
#define ATA_REG_ERROR      0x01u
#define ATA_REG_FEATURES   0x01u
#define ATA_REG_SECCOUNT0  0x02u
#define ATA_REG_LBA0       0x03u
#define ATA_REG_LBA1       0x04u
#define ATA_REG_LBA2       0x05u
#define ATA_REG_DRIVE      0x06u
#define ATA_REG_STATUS     0x07u
#define ATA_REG_COMMAND    0x07u
#define ATA_CMD_READ_PIO   0x20u
#define ATA_CMD_WRITE_PIO  0x30u
#define ATA_CMD_IDENTIFY         0xECu
#define ATA_CMD_IDENTIFY_PACKET  0xA1u

#define ATA_STS_ERR        0x01u
#define ATA_STS_DRQ        0x08u
#define ATA_STS_SRV        0x10u
#define ATA_STS_DF         0x20u
#define ATA_STS_RDY        0x40u
#define ATA_STS_BSY        0x80u

static inline uint16_t ide_base(void) {
    return IDE_PRIMARY_IO;
}

static inline uint16_t ide_ctrl(void) {
    return IDE_PRIMARY_CTRL;
}

static void ide_delay(void) {
    (void)inb(ide_ctrl());
}

static int ide_wait_not_busy(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t st = inb(ide_base() + ATA_REG_STATUS);
        if (!(st & ATA_STS_BSY)) {
            return (int)st;
        }
        ide_delay();
    }
    return -1;
}

static int ide_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t st = inb(ide_base() + ATA_REG_STATUS);
        if (st & ATA_STS_ERR) {
            return -1;
        }
        if (st & ATA_STS_DRQ) {
            return 0;
        }
        if (!(st & ATA_STS_BSY) && !(st & ATA_STS_DRQ)) {
            return -1;
        }
        ide_delay();
    }
    return -1;
}

void ide_init(void) {
    /* 软复位主通道（可选） */
    outb(ide_ctrl(), 0x04u);
    ide_delay();
    outb(ide_ctrl(), 0x00u);
    ide_delay();
    (void)ide_wait_not_busy();
}

static int ide_select(uint8_t drive, uint32_t lba_hi4) {
    if (drive > 1u) {
        return -1;
    }
    uint8_t dh = (uint8_t)(0xE0u | ((drive & 1u) << 4) | (lba_hi4 & 0x0Fu));
    outb(ide_base() + ATA_REG_DRIVE, dh);
    ide_delay();
    return 0;
}

static int ide_read_identify_words_cmd(uint8_t drive, uint16_t buf[256], uint8_t cmd) {
    ide_init();
    if (ide_select(drive, 0) != 0) {
        return -1;
    }
    outb(ide_base() + ATA_REG_SECCOUNT0, 0);
    outb(ide_base() + ATA_REG_LBA0, 0);
    outb(ide_base() + ATA_REG_LBA1, 0);
    outb(ide_base() + ATA_REG_LBA2, 0);
    outb(ide_base() + ATA_REG_COMMAND, cmd);
    ide_delay();

    uint8_t st = (uint8_t)ide_wait_not_busy();
    if (st == 0xFFu) {
        return -1;
    }
    if (st & ATA_STS_ERR) {
        return -1;
    }

    if (ide_wait_drq() != 0) {
        return -1;
    }

    for (int w = 0; w < 256; w++) {
        buf[w] = inw(ide_base() + ATA_REG_DATA);
    }
    return 0;
}

int ide_identify_packet(uint8_t drive, char model[40]) {
    if (!model) {
        return -1;
    }
    uint16_t buf[256];
    if (ide_read_identify_words_cmd(drive, buf, ATA_CMD_IDENTIFY_PACKET) != 0) {
        return -1;
    }

    for (int i = 0; i < 20; i++) {
        uint16_t v = buf[27 + i];
        model[i * 2] = (char)(v >> 8);
        model[i * 2 + 1] = (char)(v & 0xFF);
    }
    return 0;
}

int ide_identify(uint8_t drive, char model[40]) {
    if (!model) {
        return -1;
    }
    uint16_t buf[256];
    if (ide_read_identify_words_cmd(drive, buf, ATA_CMD_IDENTIFY) != 0) {
        return -1;
    }

    /* words 27-46: model string, byte-swapped per 16-bit word */
    for (int i = 0; i < 20; i++) {
        uint16_t v = buf[27 + i];
        model[i * 2] = (char)(v >> 8);
        model[i * 2 + 1] = (char)(v & 0xFF);
    }
    return 0;
}

int ide_probe_type(uint8_t drive, char model[40], int *class_out) {
    if (!model || !class_out) {
        return -1;
    }
    if (ide_identify(drive, model) == 0) {
        *class_out = IDE_CLASS_ATA;
        return 0;
    }
    if (ide_identify_packet(drive, model) == 0) {
        *class_out = IDE_CLASS_ATAPI;
        return 0;
    }
    *class_out = IDE_CLASS_NONE;
    model[0] = '\0';
    return -1;
}

int ide_capacity_sectors(uint8_t drive, uint32_t *sectors_out) {
    if (!sectors_out) {
        return -1;
    }
    uint16_t buf[256];
    if (ide_read_identify_words_cmd(drive, buf, ATA_CMD_IDENTIFY) != 0) {
        return -1;
    }
    *sectors_out = (uint32_t)buf[60] | ((uint32_t)buf[61] << 16);
    if (*sectors_out == 0u) {
        return -1;
    }
    return 0;
}

static int ide_do_rw(uint8_t drive, uint32_t lba, uint32_t count, void *buf,
                     int writing) {
    if (!buf || count == 0 || count > 256u) {
        return -1;
    }
    if (lba > 0x0FFFFFFFu) {
        return -1;
    }

    uint8_t *b = (uint8_t *)buf;
    for (uint32_t n = 0; n < count; n++) {
        uint32_t cur = lba + n;
        uint8_t lba7 = (uint8_t)((cur >> 24) & 0x0Fu);
        if (ide_select(drive, lba7) != 0) {
            return -1;
        }
        outb(ide_base() + ATA_REG_SECCOUNT0, 1);
        outb(ide_base() + ATA_REG_LBA0, (uint8_t)(cur & 0xFFu));
        outb(ide_base() + ATA_REG_LBA1, (uint8_t)((cur >> 8) & 0xFFu));
        outb(ide_base() + ATA_REG_LBA2, (uint8_t)((cur >> 16) & 0xFFu));

        uint8_t cmd = writing ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO;
        outb(ide_base() + ATA_REG_COMMAND, cmd);
        ide_delay();

        if (ide_wait_not_busy() < 0) {
            return -1;
        }
        if (writing) {
            if (ide_wait_drq() != 0) {
                return -1;
            }
            for (int w = 0; w < 256; w++) {
                uint16_t v = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
                outw(ide_base() + ATA_REG_DATA, v);
                b += 2;
            }
            if (ide_wait_not_busy() < 0) {
                return -1;
            }
        } else {
            if (ide_wait_drq() != 0) {
                return -1;
            }
            for (int w = 0; w < 256; w++) {
                uint16_t v = inw(ide_base() + ATA_REG_DATA);
                b[0] = (uint8_t)v;
                b[1] = (uint8_t)(v >> 8);
                b += 2;
            }
        }
        uint8_t st = inb(ide_base() + ATA_REG_STATUS);
        if (st & ATA_STS_ERR) {
            return -1;
        }
    }
    return 0;
}

int ide_read_sectors(uint8_t drive, uint32_t lba, uint32_t count, void *buf) {
    return ide_do_rw(drive, lba, count, buf, 0);
}

int ide_write_sectors(uint8_t drive, uint32_t lba, uint32_t count, const void *buf) {
    return ide_do_rw(drive, lba, count, (void *)buf, 1);
}

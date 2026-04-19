/* kernel/drivers/ide.h - Primary IDE channel, ATA PIO (LBA28) */

#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stddef.h>

#define IDE_PRIMARY_IO       0x1F0u
#define IDE_PRIMARY_CTRL     0x3F6u

/* drive: 0 = master, 1 = slave */

void ide_init(void);

/* IDENTIFY DEVICE；成功返回 0，model 为 40 字节 ASCII（非 0 结尾） */
int ide_identify(uint8_t drive, char model[40]);

/* IDENTIFY 扇区总数：优先 word 60-61，若为 0 则用 word 100-103（48-bit） */
int ide_capacity_sectors(uint8_t drive, uint32_t *sectors_out);

/*
 * PIO 读写若干连续扇区（每扇区 512 字节）。
 * buf 按 2 字节对齐更稳妥。
 */
int ide_read_sectors(uint8_t drive, uint32_t lba, uint32_t count, void *buf);
int ide_write_sectors(uint8_t drive, uint32_t lba, uint32_t count, const void *buf);

/* ATAPI（如 CD-ROM）：IDENTIFY PACKET DEVICE，成功则 model 同上 */
int ide_identify_packet(uint8_t drive, char model[40]);

#define IDE_CLASS_NONE   0
#define IDE_CLASS_ATA    1
#define IDE_CLASS_ATAPI  2

/* 先尝试 ATA IDENTIFY，失败再试 ATAPI；返回 0 且 *class_out 有效 */
int ide_probe_type(uint8_t drive, char model[40], int *class_out);

/*
 * ATAPI PACKET（PIO）：发送 12 字节 CDB（SCSI/ATAPI）。
 * data_in：1 = 设备→主机（读 INQUIRY、READ 等），0 = 无数据相（TEST UNIT READY 等）。
 * buf / buf_sz：数据缓冲区与最大长度；无数据相时 buf 可为 NULL、buf_sz 为 0。
 * actual_out：可选，返回实际传输字节数。
 */
int ide_atapi_packet(uint8_t drive, const uint8_t cdb[12], int data_in, void *buf, uint32_t buf_sz,
                     uint32_t *actual_out);

/* INQUIRY，默认取 36 字节标准响应 */
int ide_atapi_inquiry(uint8_t drive, void *buf, uint32_t buf_sz, uint32_t *actual_out);

/* READ(10)：按 2048 字节/块（CD-ROM 扇区）读取一块 */
int ide_atapi_read2048(uint8_t drive, uint32_t lba_blk, void *buf);

#endif

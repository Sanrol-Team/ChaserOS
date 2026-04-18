/* kernel/power.h - 软关机 / 重启（ACPI 等，仅内核态可直接 I/O） */

#ifndef KERNEL_POWER_H
#define KERNEL_POWER_H

void power_shutdown(void);
void power_reboot(void);

#endif

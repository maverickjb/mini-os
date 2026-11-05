#ifndef _DRIVERS_RTC_PL031_H
#define _DRIVERS_RTC_PL031_H

/*
 * ARM PrimeCell PL031 RTC (QEMU virt @ 0x09010000).
 * Counter is Unix seconds; 1 Hz.
 */

void pl031_init(void);
unsigned int pl031_read_seconds(void);

#endif /* _DRIVERS_RTC_PL031_H */

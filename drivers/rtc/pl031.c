/*
 * PL031 RTC — Unix-second wall clock on QEMU virt.
 */

#include <drivers/rtc/pl031.h>
#include <asm/memory.h>

#define PL031_PHYS      0x09010000UL
#define PL031_VIRT      ((unsigned long)__phys_to_virt(PL031_PHYS))

#define PL031_DR        (*(volatile unsigned int *)(PL031_VIRT + 0x00))
#define PL031_LR        (*(volatile unsigned int *)(PL031_VIRT + 0x08))
#define PL031_CR        (*(volatile unsigned int *)(PL031_VIRT + 0x0c))
#define PL031_CR_START  1U

void pl031_init(void)
{
    PL031_CR = PL031_CR_START;
}

unsigned int pl031_read_seconds(void)
{
    return PL031_DR;
}

void pl031_write_seconds(unsigned int sec)
{
    PL031_LR = sec;
}

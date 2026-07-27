#include "uart.h"

static void enable_el1_user_access(void)
{
    unsigned long sctlr;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1UL << 22); /* UAO: allow EL1 to access EL0 mappings */
    __asm__ volatile("msr sctlr_el1, %0" : : "r"(sctlr));
    __asm__ volatile("isb");
}

long ksys_write(unsigned long fd, const char *buf, unsigned long count)
{
    unsigned long i;

    if (fd != 1 && fd != 2)
        return -9;

    if (!buf)
        return -14;

    enable_el1_user_access();

    for (i = 0; i < count; i++)
        uart_putc(buf[i]);

    return (long)count;
}
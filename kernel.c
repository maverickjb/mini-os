/*
 * miniSMP kernel — AArch64 bare-metal on QEMU virt
 */

#include "uart.h"

static unsigned int cpu_id(void)
{
    unsigned long mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (unsigned int)(mpidr & 0xff);
}

void kernel_main(void)
{
    unsigned int id = cpu_id();

    if (id == 0)
        uart_puts("Hello from CPU0\n");

    for (;;)
        __asm__ volatile("wfe");
}

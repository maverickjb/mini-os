/*
 * miniSMP kernel — AArch64 bare-metal on QEMU virt
 */

#include "smp.h"
#include "time.h"
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

    if (id == 0) {
        uart_puts("Hello from CPU0\n");
        smp_init();
        bringup_nonboot_cpus();

        for (unsigned int i = 1; i < NR_CPUS; i++) {
            uart_puts("Hello from CPU");
            uart_putc('0' + (char)i);
            uart_puts("\n");
        }

        time_init();
        uart_puts("Tick timer started\n");
    }

    for (;;)
        __asm__ volatile("wfi");
}

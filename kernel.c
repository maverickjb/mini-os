/*
 * miniSMP kernel — AArch64 bare-metal on QEMU virt
 */

#include "page_alloc.h"
#include "ramfs.h"
#include "initramfs.h"
#include "smp.h"
#include "sched.h"
#include "uart.h"

extern char __initramfs_start[];
extern char __initramfs_end[];

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

        page_alloc_init();

        ramfs_init();

        if ((unsigned long)__initramfs_end > (unsigned long)__initramfs_start) {
            unsigned long size = (unsigned long)(__initramfs_end -
                                                   __initramfs_start);
            int err = unpack_to_rootfs(__initramfs_start, size);

            if (err)
                uart_puts("unpack_to_rootfs failed\n");
            else
                uart_puts("unpack_to_rootfs: ok\n");
        }

        sched_init();
        rest_init();

        for (unsigned int i = 0; i < NR_CPUS; i++) {
            uart_puts("CPU");
            uart_putc('0' + (char)i);
            uart_puts(" idle task (PID 0) ready\n");
        }
    }

    cpu_idle();
}

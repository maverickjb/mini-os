/*
 * SMP bring-up — Linux-inspired names on bare-metal QEMU virt.
 *
 * On QEMU virt all CPUs enter _start together. Secondaries park in boot.S
 * until cpu_up() writes cpu_release[] and sends SEV.
 */

#include <linux/sched.h>
#include <linux/errno.h>
 
#include "mem.h"
#include "smp.h"

extern void secondary_startup(void);
extern void mmu_enable_secondary(void);

volatile unsigned long cpu_release[NR_CPUS];
static volatile unsigned char cpu_online[NR_CPUS];

unsigned int smp_processor_id(void)
{
    unsigned long mpidr;

    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (unsigned int)(mpidr & 0xff);
}

static unsigned int cpu_id(void)
{
    return smp_processor_id();
}

void smp_init(void)
{
    for (unsigned int i = 0; i < NR_CPUS; i++) {
        cpu_release[i] = 0;
        cpu_online[i] = 0;
    }

    cpu_online[0] = 1;
}

int cpu_up(unsigned int cpu)
{
    if (cpu == 0 || cpu >= NR_CPUS)
        return -EINVAL;

    if (cpu_online[cpu])
        return 0;

    cpu_release[cpu] = __virt_to_phys(secondary_startup);
    __asm__ volatile("dsb sy");
    __asm__ volatile("sev");

    for (unsigned int spins = 0; !cpu_online[cpu]; spins++) {
        if (spins == 100000000U)
            return -ETIMEDOUT;
        __asm__ volatile("yield");
    }

    return 0;
}

void bringup_nonboot_cpus(void)
{
    for (unsigned int cpu = 1; cpu < NR_CPUS; cpu++)
        (void)cpu_up(cpu);
}

void secondary_main(void)
{
    unsigned int id = cpu_id();

    cpu_online[id] = 1;
    sched_init_idle(id);
    cpu_idle();
}

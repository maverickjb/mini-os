/*
 * SMP bring-up via PSCI CPU_ON (QEMU virt).
 *
 * CPU0 boots from _start; secondaries stay powered off in firmware until
 * psci_cpu_on() starts them at secondary_startup (MMU off).
 */

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sched/task.h>
#include <linux/printk.h>

#include <asm/memory.h>
#include <asm/smp.h>
#include <asm/psci.h>
#include <asm/processor.h>

/* Entry stub in head.S — entered with MMU off at its physical address. */
extern void secondary_startup(void);

unsigned int nr_cpus = NR_CPUS;

static volatile unsigned char cpu_online[NR_CPUS];

unsigned int smp_processor_id(void)
{
    unsigned long mpidr;

    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (unsigned int)(mpidr & 0xff);
}

void smp_init(void)
{
    unsigned int i;

    for (i = 0; i < nr_cpus; i++) {
        cpu_online[i] = 0;
        sched_init_idle(i);
    }

    cpu_online[0] = 1;
}

#define CPU_UP_TIMEOUT  1000000U

static int wait_cpu_online(unsigned int cpu)
{
    unsigned int spins = 0;

    while (!cpu_online[cpu]) {
        if (++spins >= CPU_UP_TIMEOUT)
            return -ETIMEDOUT;

        cpu_relax();
    }

    return 0;
}

int cpu_up(unsigned int cpu)
{
    unsigned long entry;
    int ret;

    if (cpu == 0 || cpu >= nr_cpus)
        return -EINVAL;

    if (cpu_online[cpu])
        return 0;

    entry = __virt_to_phys((unsigned long)secondary_startup);
    ret = psci_cpu_on(cpu, entry);
    if (ret) {
        pr_err("CPU%u: psci_cpu_on failed: %d\n", cpu, ret);
        return ret;
    }

    ret = wait_cpu_online(cpu);
    if (ret)
        pr_err("CPU%u: timed out waiting to come online\n", cpu);

    return ret;
}

void bringup_nonboot_cpus(void)
{
    unsigned int cpu;

    for (cpu = 1; cpu < nr_cpus; cpu++)
        (void)cpu_up(cpu);
}

void secondary_main(void)
{
    unsigned int id = smp_processor_id();

    pr_info("CPU%u: Hello\n", id);

    cpu_online[id] = 1;

    for (;;)
        asm volatile("wfi");
}

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

static volatile unsigned char cpu_online_map[NR_CPUS];

struct cpu cpu_data[NR_CPUS];

static void cpu_mark_online(unsigned int cpu)
{
    cpu_online_map[cpu] = 1;
}

static void cpu_init(unsigned int cpu)
{
    cpu_data[cpu].id = cpu;
    cpu_data[cpu].idle = &idle_tasks[cpu];
    cpu_data[cpu].curr = &idle_tasks[cpu];
    sched_init_idle(cpu);
}

void smp_init(void)
{
    unsigned int cpu;

    for (cpu = 0; cpu < NR_CPUS; cpu++) {
        cpu_online_map[cpu] = 0;
        cpu_init(cpu);
    }

    cpu_mark_online(0);

    pr_info("SMP: boot CPU is CPU0\n");
}

#define CPU_UP_TIMEOUT  1000000U

static int wait_cpu_online(unsigned int cpu)
{
    unsigned int spins = 0;

    while (!cpu_online_map[cpu]) {
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

    if (cpu == 0 || cpu >= NR_CPUS)
        return -EINVAL;

    if (cpu_online_map[cpu])
        return 0;

    entry = __virt_to_phys((unsigned long)secondary_startup);
    ret = psci_cpu_on(cpu, entry);
    if (ret) {
        pr_err("CPU%u: psci_cpu_on failed: %d\n", cpu, ret);
        return ret;
    }

    ret = wait_cpu_online(cpu);
    if (ret) {
        pr_err("CPU%u: failed to come online: %d\n", cpu, ret);
        return ret;
    }

    pr_info("CPU%u: online\n", cpu);

    return 0;
}

void bringup_nonboot_cpus(void)
{
    unsigned int cpu;
    int ret;

    for (cpu = 1; cpu < NR_CPUS; cpu++) {
        ret = cpu_up(cpu);
        if (ret) {
            pr_err("CPU%u: failed to come online: %d\n", cpu, ret);
            return;
        }
    }
}

void secondary_main(void)
{
    unsigned int cpu = smp_processor_id();

    pr_info("CPU%u: secondary CPU started\n", cpu);

    cpu_mark_online(cpu);

    cpu_idle();
}

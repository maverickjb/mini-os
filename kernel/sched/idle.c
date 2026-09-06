/*
 * Idle task — PID 0 per CPU.
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/printk.h>

#include <asm/smp.h>
#include <asm/irqflags.h>

void cpu_idle(void)
{
    unsigned int cpu = smp_processor_id();
    struct cpu *c = &cpu_data[cpu];
    struct task_struct *idle = c->idle;

    set_current(idle);
    idle->state = TASK_IDLE;

    /*
     * Idle runs with IRQs on so timer SGIs/CNTP wake WFI through the
     * real IRQ path (not schedule()-polling alone). Update idle->daif
     * too: context_switch restores it when returning to idle, and the
     * initial 0x3c0 value would otherwise remask IRQs on every switch
     * back to the idle task.
     */
    local_irq_enable();
    __asm__ volatile("mrs %0, daif" : "=r"(idle->daif));

    pr_info("CPU%u: idle task running\n", cpu);

    for (;;) {
        schedule();
        if (get_current() == idle)
            __asm__ volatile("wfi");
    }
}

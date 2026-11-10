/*
 * Idle task — PID 0 per CPU.
 *
 * Polling protocol (simplified Linux TIF_POLLING_NRFLAG):
 * idle_polling[cpu] means this CPU's idle loop will observe need_resched
 * without an IPI. Drop the flag before WFI so a remote wake sends SGI.
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/printk.h>

#include <asm/smp.h>
#include <asm/irqflags.h>
#include <asm/barrier.h>

void cpu_idle(void)
{
    unsigned int cpu = smp_processor_id();
    struct cpu *c = &cpu_data[cpu];
    struct task_struct *idle = c->idle;

    set_current(idle);
    idle->state = TASK_IDLE;
    idle_polling[cpu] = 0;

    /*
     * Saved daif must have IRQs on: context_switch restores it when we
     * become idle again. The loop below masks IRQs around the poll/WFI
     * handshake; schedule() is called with IRQs enabled.
     */
    local_irq_enable();
    __asm__ volatile("mrs %0, daif" : "=r"(idle->daif));

    pr_info("CPU%u: idle task running\n", cpu);

    for (;;) {
        local_irq_disable();
        idle_polling[cpu] = 1;
        smp_mb();
        if (need_resched()) {
            idle_polling[cpu] = 0;
            local_irq_enable();
            schedule();
            continue;
        }

        /* Not polling: wakers must IPI. Recheck after the store. */
        idle_polling[cpu] = 0;
        smp_mb();
        if (need_resched()) {
            local_irq_enable();
            schedule();
            continue;
        }

        /* IRQs off, polling clear, no work — WFI until SGI/timer. */
        __asm__ volatile("wfi" : : : "memory");
        local_irq_enable();
    }
}

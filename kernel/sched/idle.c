/*
 * Idle task — PID 0 per CPU.
 *
 * TIF_POLLING_NRFLAG on this CPU's idle task_struct means the idle loop
 * will observe TIF_NEED_RESCHED without an IPI. Clear it before WFI so a
 * remote wake sends SGI. resched_cpu() uses set_nr_and_not_polling().
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
    clear_tsk_flag(idle, TIF_POLLING_NRFLAG);

    /*
     * schedule() is called with IRQs enabled so the incoming `flags`
     * local restores I-clear. The loop masks around the poll/WFI handshake.
     */
    local_irq_enable();

    pr_info("CPU%u: idle task running\n", cpu);

    for (;;) {
        local_irq_disable();
        set_tsk_flag(idle, TIF_POLLING_NRFLAG);
        smp_mb();
        if (need_resched() || c->rq.nr_running) {
            clear_tsk_flag(idle, TIF_POLLING_NRFLAG);
            local_irq_enable();
            schedule();
            continue;
        }

        /* Not polling: wakers must IPI. Recheck after the store. */
        clear_tsk_flag(idle, TIF_POLLING_NRFLAG);
        smp_mb();
        if (need_resched() || c->rq.nr_running) {
            local_irq_enable();
            schedule();
            continue;
        }

        /*
         * WFI with IRQs on so a remote SGI is taken. QEMU/cortex-a72
         * may not complete WFI for DAIF-masked interrupts, which left
         * ash sitting in ppoll until wake_jiffies (1–2s).
         */
        local_irq_enable();
        __asm__ volatile("wfi" : : : "memory");
    }
}

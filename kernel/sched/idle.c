/*
 * Idle task — PID 0 per CPU.
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/printk.h>

#include <asm/smp.h>

void cpu_idle(void)
{
    unsigned int cpu = smp_processor_id();
    struct cpu *c = &cpu_data[cpu];
    struct task_struct *idle = c->idle;

    set_current(idle);
    idle->state = TASK_IDLE;

    pr_info("CPU%u: idle task running\n", cpu);

    for (;;) {
//        if (cpu == 0)
            schedule();
        if (get_current() == idle)
            __asm__ volatile("wfi");
    }
}

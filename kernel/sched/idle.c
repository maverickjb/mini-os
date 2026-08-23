/*
 * Idle task — PID 0 per CPU.
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/serial.h>

#include <asm/smp.h>

void cpu_idle(void)
{
    unsigned int cpu = smp_processor_id();
    struct task_struct *idle = &idle_tasks[cpu];

    set_current(idle);
    idle->state = TASK_IDLE;

    if (cpu == 0) {
        uart_puts("CPU");
        uart_putc('0' + (char)cpu);
        uart_puts(" idle task (PID 0) running\n");
    }

    for (;;) {
        if (cpu == 0)
            schedule();
        if (get_current() == idle)
            __asm__ volatile("wfi");
    }
}

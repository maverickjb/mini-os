/*
 * Scheduler — idle tasks (PID 0) and minimal run queue.
 */

#include <linux/sched/task.h>
#include <linux/serial.h>

#include "smp.h"

extern void cpu_switch_to(struct task_struct *prev, struct task_struct *next);

struct task_struct idle_tasks[NR_CPUS] = {
    [0] = { .pid = 0, .state = TASK_IDLE },
    [1] = { .pid = 0, .state = TASK_IDLE },
    [2] = { .pid = 0, .state = TASK_IDLE },
    [3] = { .pid = 0, .state = TASK_IDLE },
};
static struct task_struct *cpu_current[NR_CPUS];
static struct task_struct *runqueue;

struct task_struct *get_current(void)
{
    return cpu_current[smp_processor_id()];
}

void set_current(struct task_struct *task)
{
    cpu_current[smp_processor_id()] = task;
}

void enqueue_task(struct task_struct *task)
{
    struct task_struct *walk;

    task->next = 0;
    task->state = TASK_RUNNING;

    if (!runqueue) {
        runqueue = task;
        return;
    }

    walk = runqueue;
    while (walk->next)
        walk = walk->next;

    walk->next = task;
}

struct task_struct *pick_next_task(struct task_struct *prev)
{
    struct task_struct *start;
    struct task_struct *walk;

    if (!runqueue)
        return prev;

    start = prev && prev->next ? prev->next : runqueue;
    walk = start;

    do {
        if (walk->state == TASK_RUNNING && walk->is_user)
            return walk;
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);

    return prev;
}

void sched_init(void)
{
    sched_init_idle(0);
}

void sched_init_idle(unsigned int cpu)
{
    cpu_current[cpu] = &idle_tasks[cpu];
}

void rest_init(void)
{
    struct task_struct *init;

    init = kernel_thread(kernel_init, 0);
    if (!init) {
        uart_puts("kernel_thread failed\n");
        return;
    }

    wake_up_process(init);
    runqueue = init;

    uart_puts("Rest init: PID 1 created, boot thread -> idle\n");
}

__attribute__((noinline))
void schedule(void)
{
    struct task_struct *prev = get_current();
    struct task_struct *next;
    unsigned int cpu = smp_processor_id();

    if (cpu != 0)
        return;

    if (prev == &idle_tasks[cpu]) {
        if (!runqueue || runqueue->state != TASK_RUNNING)
            return;
        next = runqueue;
    } else if (prev->is_user) {
        return;
    } else {
        next = &idle_tasks[cpu];
    }

    if (next == prev)
        return;

    if (prev->pid == 0)
        prev->state = TASK_IDLE;
    else
        prev->state = TASK_RUNNING;

    if (next->pid == 0)
        next->state = TASK_IDLE;
    else
        next->state = TASK_RUNNING;

    set_current(next);
    cpu_switch_to(prev, next);
}

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
        if (cpu == 0 && runqueue && runqueue->state == TASK_RUNNING)
            schedule();
        else
            __asm__ volatile("wfi");
    }
}

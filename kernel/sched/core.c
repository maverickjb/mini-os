/*
 * Round-robin scheduler — PID 0 idle, runnable tasks time-sliced.
 */

#include <linux/sched/task.h>
#include <linux/serial.h>
#include <linux/stddef.h>
#include <linux/sched.h>

#include "smp.h"

extern void cpu_switch_to(struct task_struct *prev, struct task_struct *next);

struct task_struct idle_tasks[NR_CPUS] = {
    [0] = { .pid = 0, .state = TASK_IDLE },
    [1] = { .pid = 0, .state = TASK_IDLE },
    [2] = { .pid = 0, .state = TASK_IDLE },
    [3] = { .pid = 0, .state = TASK_IDLE },
};

static struct task_struct *cpu_current[NR_CPUS];
struct task_struct *runqueue;

struct task_struct *get_current(void)
{
    return cpu_current[smp_processor_id()];
}

void set_current(struct task_struct *task)
{
    cpu_current[smp_processor_id()] = task;
}

static struct task_struct *idle_task(void)
{
    return &idle_tasks[smp_processor_id()];
}

void enqueue_task(struct task_struct *task)
{
    struct task_struct *walk;

    task->next = NULL;
    task->state = TASK_RUNNING;
    task->time_slice = SCHED_TIME_SLICE;

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
        return idle_task();

    if (prev && prev != idle_task() && prev->next)
        start = prev->next;
    else
        start = runqueue;

    walk = start;

    do {
        if (walk->state == TASK_RUNNING)
            return walk;
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);

    return idle_task();
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

static void switch_kernel_tasks(struct task_struct *prev, struct task_struct *next)
{
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

    next->time_slice = SCHED_TIME_SLICE;
    set_current(next);
    cpu_switch_to(prev, next);
}

__attribute__((noinline))
void schedule(struct pt_regs *regs)
{
    struct task_struct *prev = current;
    struct task_struct *next;

    next = pick_next_task(prev);

    if (next == prev)
        return;

    if (next->is_user) {
        save_user_regs(prev, regs);
        next->time_slice = SCHED_TIME_SLICE;
        set_current(next);
        restore_user_regs(next, regs);
        return;
    }

    switch_kernel_tasks(prev, next);
}

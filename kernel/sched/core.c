/*
 * Round-robin scheduler — PID 0 idle, runnable tasks time-sliced.
 */

#include <linux/sched/task.h>
#include <linux/serial.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <asm/exception.h>

#include <asm/smp.h>

#include <linux/mm.h>

struct task_struct idle_tasks[NR_CPUS] = {
    [0] = { .pid = 0, .state = TASK_IDLE },
    [1] = { .pid = 0, .state = TASK_IDLE },
    [2] = { .pid = 0, .state = TASK_IDLE },
    [3] = { .pid = 0, .state = TASK_IDLE },
};

static struct task_struct *cpu_current[NR_CPUS];
struct task_struct *runqueue;

/* Exported for prepare_return_to_el0 in context.S (UP: CPU0 only). */
struct task_struct *cpu_current_export;

struct task_struct *get_current(void)
{
    return cpu_current[smp_processor_id()];
}

void set_current(struct task_struct *task)
{
    unsigned int cpu = smp_processor_id();

    cpu_current[cpu] = task;
    if (cpu == 0)
        cpu_current_export = task;
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

void dequeue_task(struct task_struct *task)
{
    struct task_struct **prev;

    if (!task)
        return;

    prev = &runqueue;
    while (*prev) {
        if (*prev == task) {
            *prev = task->next;
            task->next = NULL;
            return;
        }
        prev = &(*prev)->next;
    }
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
    if (cpu == 0)
        cpu_current_export = &idle_tasks[cpu];
}

static void context_switch(struct task_struct *prev, struct task_struct *next,
                           struct pt_regs *regs)
{
    if (prev->is_user && interrupted_el0(regs))
        save_user_regs(prev, regs);

    next->time_slice = SCHED_TIME_SLICE;

    set_current(next);

    if (next->is_user && next->mm)
        mm_install(next->mm);

    /*
     * Real context switch — returns on prev's kernel stack only when prev
     * runs again (Linux-style). On exit, prev is zombie and never returns.
     */
    switch_to(prev, next);
}

void schedule(struct pt_regs *regs)
{
    struct task_struct *prev = current;
    struct task_struct *next;

    next = pick_next_task(prev);

    if (next == prev)
        return;

    context_switch(prev, next, regs);
}

/*
 * Round-robin scheduler — PID 0 idle, runnable tasks time-sliced.
 *
 * schedule() does not take a trap frame: pt_regs lives on the current
 * task's kernel stack (pointed to by task->regs). switch_to() saves SP,
 * so the frame remains in place until the task runs again and returns
 * through the syscall/IRQ exit path.
 */

#include <linux/sched/task.h>
#include <linux/serial.h>
#include <linux/stddef.h>
#include <linux/sched.h>

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

/* Exported for prepare_kstack_el0 in assembler (UP: CPU0 only). */
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
    idle_tasks[cpu].daif = 0x3c0UL; /* masked until idle enables IRQs */
    cpu_current[cpu] = &idle_tasks[cpu];
    if (cpu == 0)
        cpu_current_export = &idle_tasks[cpu];
}

static void context_switch(struct task_struct *prev, struct task_struct *next)
{
    unsigned long daif;

    if (prev->is_user)
        __asm__ volatile("mrs %0, sp_el0" : "=r"(prev->user_sp));

    /*
     * DAIF is per-CPU PSTATE, not saved by switch_to. Without this, a task
     * that local_irq_enable()'s before schedule() can resume another task
     * mid-IRQ-exit with IRQs on, allowing a nested IRQ to clobber ELR/SPSR.
     */
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    prev->daif = daif;

    next->time_slice = SCHED_TIME_SLICE;

    set_current(next);
    __asm__ volatile("msr daif, %0" : : "r"(next->daif) : "memory");

    if (next->is_user && next->mm) {
        mm_install(next->mm);
        /*
         * SP_EL0 is per-CPU, not per-task. Restore the next task's saved
         * user stack pointer so a later eret does not reuse the previous
         * task's SP (e.g. parent waitpid after child exec/exit).
         */
        __asm__ volatile("msr sp_el0, %0" : : "r"(next->user_sp));
    }

    /*
     * Real context switch — returns on prev's kernel stack only when prev
     * runs again (Linux-style). On exit, prev is zombie and never returns.
     * prev's SP still points into the stack that holds its pt_regs.
     */
    switch_to(prev, next);
}

void schedule(void)
{
    struct task_struct *prev = current;
    struct task_struct *next;

    next = pick_next_task(prev);

    if (next == prev)
        return;

    context_switch(prev, next);
}

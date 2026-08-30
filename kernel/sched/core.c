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
#include <linux/spinlock.h>
#include <linux/list.h>

#include <asm/smp.h>

#include <linux/mm.h>

struct task_struct idle_tasks[NR_CPUS] = {
    [0] = { .pid = 0, .state = TASK_IDLE },
    [1] = { .pid = 0, .state = TASK_IDLE },
    [2] = { .pid = 0, .state = TASK_IDLE },
    [3] = { .pid = 0, .state = TASK_IDLE },
};

static struct task_struct *cpu_current[NR_CPUS];
struct rq cpu_rq = {
    .lock = SPINLOCK_INIT,
    .tasks = { &cpu_rq.tasks, &cpu_rq.tasks },
    .curr = NULL,
    .nr_running = 0,
};

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

static int task_on_rq(struct list_head *list)
{
    return list->next != NULL && list->next != list;
}

void rq_init(struct rq *rq)
{
    spin_lock_init(&rq->lock);
    INIT_LIST_HEAD(&rq->tasks);
    rq->curr = NULL;
    rq->nr_running = 0;
}

static void enqueue_task_locked(struct task_struct *task)
{
    if (task_on_rq(&task->run_list))
        return;

    task->state = TASK_RUNNING;
    task->time_slice = SCHED_TIME_SLICE;
    list_add_tail(&task->run_list, &cpu_rq.tasks);
    cpu_rq.nr_running++;
}

void enqueue_task(struct task_struct *task)
{
    unsigned long flags;

    spin_lock_irqsave(&cpu_rq.lock, flags);
    enqueue_task_locked(task);
    spin_unlock_irqrestore(&cpu_rq.lock, flags);
}

static void dequeue_task_locked(struct task_struct *task)
{
    if (!task || !task_on_rq(&task->run_list))
        return;

    list_del_init(&task->run_list);
    if (cpu_rq.nr_running)
        cpu_rq.nr_running--;
}

void dequeue_task(struct task_struct *task)
{
    unsigned long flags;

    spin_lock_irqsave(&cpu_rq.lock, flags);
    dequeue_task_locked(task);
    spin_unlock_irqrestore(&cpu_rq.lock, flags);
}

struct task_struct *pick_next_task(struct task_struct *prev)
{
    struct list_head *head = &cpu_rq.tasks;
    struct list_head *pos;
    struct list_head *start;

    if (list_empty(head))
        return idle_task();

    if (prev && prev != idle_task() && task_on_rq(&prev->run_list))
        start = prev->run_list.next;
    else
        start = head->next;

    pos = start;
    while (1) {
        struct task_struct *task;

        if (pos == head) {
            pos = pos->next;
            if (pos == start)
                break;
            continue;
        }

        task = list_entry(pos, struct task_struct, run_list);
        if (task->state == TASK_RUNNING)
            return task;

        pos = pos->next;
        if (pos == start)
            break;
    }

    return idle_task();
}

void sched_init(void)
{
    sched_init_idle(0);
}

void sched_init_idle(unsigned int cpu)
{
    idle_tasks[cpu].daif = 0x3c0UL; /* masked until idle enables IRQs */
    INIT_LIST_HEAD(&idle_tasks[cpu].run_list);
    cpu_current[cpu] = &idle_tasks[cpu];
    if (cpu == 0)
        cpu_current_export = &idle_tasks[cpu];
}

static void context_switch(struct task_struct *prev, struct task_struct *next)
{
    unsigned long daif;

    if (prev->is_user) {
        __asm__ volatile("mrs %0, sp_el0" : "=r"(prev->user_sp));
        /* Userspace (musl) writes TPIDR_EL0 for TLS; keep it per-task. */
        __asm__ volatile("mrs %0, tpidr_el0" : "=r"(prev->tpidr_el0));
    }

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
         * SP_EL0 / TPIDR_EL0 are per-CPU, not per-task. Restore the next
         * task's saved values so parent waitpid after child exec/exit does
         * not keep the child's TLS base (would break __errno_location).
         */
        __asm__ volatile("msr sp_el0, %0" : : "r"(next->user_sp));
        __asm__ volatile("msr tpidr_el0, %0" : : "r"(next->tpidr_el0));
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
    unsigned long flags;

    spin_lock_irqsave(&cpu_rq.lock, flags);
    cpu_rq.curr = prev;
    next = pick_next_task(prev);
    spin_unlock_irqrestore(&cpu_rq.lock, flags);

    if (next == prev)
        return;

    context_switch(prev, next);
}

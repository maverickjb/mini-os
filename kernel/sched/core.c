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
#include <linux/errno.h>
#include <linux/printk.h>

#include <asm/smp.h>

#include <linux/mm.h>

struct task_struct idle_tasks[NR_CPUS] = {
    [0] = { .pid = 0, .state = TASK_IDLE },
    [1] = { .pid = 0, .state = TASK_IDLE },
    [2] = { .pid = 0, .state = TASK_IDLE },
    [3] = { .pid = 0, .state = TASK_IDLE },
};

static spinlock_t tasklist_lock = SPINLOCK_INIT;
static struct list_head all_tasks = LIST_HEAD_INIT(all_tasks);

/* Exported for prepare_kstack_el0 in assembler (UP: CPU0 only). */
struct task_struct *cpu_current_export;

struct task_struct *get_current(void)
{
    return cpu_data[smp_processor_id()].curr;
}

void set_current(struct task_struct *task)
{
    unsigned int cpu = smp_processor_id();

    cpu_data[cpu].curr = task;
    if (cpu == 0)
        cpu_current_export = task;
}

static struct task_struct *idle_task(void)
{
    return cpu_data[smp_processor_id()].idle;
}

void rq_init(struct rq *rq, unsigned int cpu)
{
    spin_lock_init(&rq->lock);
    INIT_LIST_HEAD(&rq->tasks);
    rq->nr_running = 0;
	rq->cpu = cpu;
}

void task_list_lock_irqsave(unsigned long *flags)
{
    spin_lock_irqsave(&tasklist_lock, *flags);
}

void task_list_unlock_irqrestore(unsigned long flags)
{
    spin_unlock_irqrestore(&tasklist_lock, flags);
}

struct list_head *task_list_head(void)
{
    return &all_tasks;
}

void task_attach(struct task_struct *task)
{
    unsigned long flags;

    spin_lock_irqsave(&tasklist_lock, flags);
    if (!list_is_linked(&task->task_list))
        list_add_tail(&task->task_list, &all_tasks);
    spin_unlock_irqrestore(&tasklist_lock, flags);
}

void task_detach(struct task_struct *task)
{
    unsigned long flags;

    spin_lock_irqsave(&tasklist_lock, flags);
    if (list_is_linked(&task->task_list))
        list_del_init(&task->task_list);
    spin_unlock_irqrestore(&tasklist_lock, flags);
}

void sched_block(enum task_state state)
{
    struct task_struct *task = current;

    task->state = state;
    dequeue_task(task);
}

static void enqueue_task_locked(struct rq *rq,
                                struct task_struct *task,
                                unsigned int cpu)
{
    if (!task)
        return;

    if (list_is_linked(&task->run_list))
        return;

    task->state = TASK_RUNNING;
    task->time_slice = SCHED_TIME_SLICE;
    task->cpu = cpu;

    list_add_tail(&task->run_list, &rq->tasks);
    rq->nr_running++;
}

void enqueue_task_cpu(struct task_struct *task, unsigned int cpu)
{
    struct rq *rq;
    unsigned long flags;

    if (!task)
        return;

    if (cpu >= NR_CPUS)
        return;

    rq = &cpu_data[cpu].rq;

    spin_lock_irqsave(&rq->lock, flags);
    enqueue_task_locked(rq, task, cpu);
    spin_unlock_irqrestore(&rq->lock, flags);
}

void enqueue_task(struct task_struct *task)
{
    if (!task)
        return;

    if (task->cpu >= NR_CPUS)
        task->cpu = smp_processor_id();

    enqueue_task_cpu(task, task->cpu);
}

static void dequeue_task_locked(struct rq *rq,
                                struct task_struct *task)
{
    if (!task || !list_is_linked(&task->run_list))
        return;

    list_del_init(&task->run_list);
    if (rq->nr_running)
        rq->nr_running--;
}

void dequeue_task(struct task_struct *task)
{
    unsigned int cpu;
    struct rq *rq;
    unsigned long flags;

    if (!task)
        return;

    cpu = task->cpu;
    if (cpu >= NR_CPUS)
        return;

    rq = &cpu_data[cpu].rq;

    spin_lock_irqsave(&rq->lock, flags);
    dequeue_task_locked(rq, task);
    spin_unlock_irqrestore(&rq->lock, flags);
}

int migrate_task(struct task_struct *task, unsigned int new_cpu)
{
    unsigned int old_cpu;
    struct rq *old_rq;
    struct rq *new_rq;
    unsigned long flags;

    if (!task)
        return -EINVAL;

    if (new_cpu >= NR_CPUS)
        return -EINVAL;

    old_cpu = task->cpu;

    if (old_cpu >= NR_CPUS)
        return -EINVAL;

    if (old_cpu == new_cpu)
        return 0;

    old_rq = &cpu_data[old_cpu].rq;
    new_rq = &cpu_data[new_cpu].rq;

    /*
     * Lock both runqueues in CPU-number order.
     * This prevents deadlock when two CPUs migrate tasks
     * in opposite directions.
     */
    if (old_cpu < new_cpu) {
        spin_lock_irqsave(&old_rq->lock, flags);
        spin_lock(&new_rq->lock);
    } else {
        spin_lock_irqsave(&new_rq->lock, flags);
        spin_lock(&old_rq->lock);
    }

    /*
     * Only migrate a task that is currently runnable.
     */
    if (task->state != TASK_RUNNING ||
        !list_is_linked(&task->run_list)) {
        if (old_cpu < new_cpu) {
            spin_unlock(&new_rq->lock);
            spin_unlock_irqrestore(&old_rq->lock, flags);
        } else {
            spin_unlock(&old_rq->lock);
            spin_unlock_irqrestore(&new_rq->lock, flags);
        }

        return -EINVAL;
    }

    list_del_init(&task->run_list);

    if (old_rq->nr_running)
        old_rq->nr_running--;

    task->cpu = new_cpu;

    list_add_tail(&task->run_list, &new_rq->tasks);
    new_rq->nr_running++;

    if (old_cpu < new_cpu) {
        spin_unlock(&new_rq->lock);
        spin_unlock_irqrestore(&old_rq->lock, flags);
    } else {
        spin_unlock(&old_rq->lock);
        spin_unlock_irqrestore(&new_rq->lock, flags);
    }

    return 0;
}

void dump_rq(struct rq *rq)
{
    struct list_head *pos;
    struct task_struct *task;

    if (!rq)
        return;

    pr_info("CPU %u: nr_running=%u\n",
            rq->cpu, rq->nr_running);

    list_for_each(pos, &rq->tasks) {
        task = list_entry(pos, struct task_struct, run_list);

        pr_info("  PID=%d cpu=%u state=%d\n",
                task->pid,
                task->cpu,
                task->state);
    }
}

struct task_struct *pick_next_task(struct rq *rq,
                                   struct task_struct *prev)
{
    struct list_head *head = &rq->tasks;
    struct list_head *pos;
    struct task_struct *idle = idle_task();

    if (list_empty(head))
        return idle;

    if (prev && prev != idle && list_is_linked(&prev->run_list))
        pos = prev->run_list.next;
    else
        pos = head->next;

    if (pos == head)
        pos = head->next;

    return list_entry(pos, struct task_struct, run_list);
}

void sched_init(void)
{

}

void sched_init_idle(unsigned int cpu)
{
    idle_tasks[cpu].daif = 0x3c0UL; /* masked until idle enables IRQs */
    idle_tasks[cpu].cpu = cpu;
    INIT_LIST_HEAD(&idle_tasks[cpu].run_list);
    cpu_data[cpu].idle = &idle_tasks[cpu];
    cpu_data[cpu].curr = &idle_tasks[cpu];
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
    next->need_resched = 0;

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
    unsigned int cpu = smp_processor_id();
    struct rq *rq = &cpu_data[cpu].rq;
    struct task_struct *prev = get_current();
    struct task_struct *next;
    unsigned long flags;

    spin_lock_irqsave(&rq->lock, flags);
    /*
     * Clear under the rq lock (IRQs off). Clearing earlier races with a
     * tick that sets need_resched again before we pick; leaving it set
     * across a switch makes irq_exit() schedule() forever when prev
     * resumes.
     */
    clear_need_resched();
    next = pick_next_task(rq, prev);
    spin_unlock_irqrestore(&rq->lock, flags);

    if (next == prev)
        return;

    context_switch(prev, next);
}


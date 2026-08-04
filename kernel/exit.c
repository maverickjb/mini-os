/*
 * Process exit — terminate a user task and switch to the next runnable task.
 */

#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/serial.h>
#include <linux/stddef.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <asm/irqflags.h>
#include <linux/mm.h>

#include <linux/gfp.h>

static void exit_files(struct task_struct *task)
{
    unsigned int i;

    for (i = 0; i < NR_OPEN; i++)
        task->files[i] = NULL;
}

static void exit_mm(struct task_struct *task)
{
    struct mm_struct *mm = task->mm;

    task->mm = NULL;
    mm_put(mm);
}

static struct task_struct *find_child(struct task_struct *parent, long pid,
                                      int want_state)
{
    struct task_struct *walk;
    struct task_struct *start;

    if (!runqueue || !parent)
        return NULL;

    walk = start = runqueue;

    do {
        if (walk->parent == parent && walk->pid == (unsigned long)pid) {
            if (want_state < 0 || walk->state == (enum task_state)want_state)
                return walk;
        }
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);

    return NULL;
}

static void free_task(struct task_struct *task)
{
    if (!task)
        return;

    dequeue_task(task);

    if (task->stack)
        free_pages(task->stack, 0);
    free_pages(task, 0);
}

void ksys_exit(struct pt_regs *regs, long status)
{
    struct task_struct *task = current;
    struct task_struct *parent;

    if (!task || !task->is_user)
        return;

    task->exit_code = status;
    exit_files(task);
    exit_mm(task);
    task->state = TASK_ZOMBIE;

    parent = task->parent;
    if (parent && parent->state == TASK_SLEEPING)
        wake_up_process(parent);

    local_irq_enable();
    schedule(regs);

    uart_puts("ksys_exit: schedule returned (bug)\n");
    for (;;)
        __asm__ volatile("wfi");
}

long ksys_wait4(struct pt_regs *regs, long pid, int *status, long options)
{
    struct task_struct *parent = current;
    struct task_struct *child;

    (void)options;

    if (!parent || !parent->is_user)
        return -EINVAL;

    if (pid <= 0)
        return -EINVAL;

    for (;;) {
        child = find_child(current, pid, TASK_ZOMBIE);
        if (child) {
            long ret = (long)child->pid;

            if (status) {
                int code = (int)child->exit_code;

                if (copy_to_user(status, &code, sizeof(code)))
                    return -EFAULT;
            }

            free_task(child);
            return ret;
        }

        if (!find_child(current, pid, -1))
            return -ECHILD;

        current->state = TASK_SLEEPING;
        local_irq_enable();
        schedule(regs);
        local_irq_disable();
        current->state = TASK_RUNNING;
        current->time_slice = SCHED_TIME_SLICE;
    }
}

/*
 * Process exit — terminate a user task and switch to the next runnable task.
 */

#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/serial.h>
#include <linux/stddef.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <uapi/linux/wait.h>
#include <asm/irqflags.h>
#include <linux/mm.h>
#include <linux/fs.h>

#include <linux/gfp.h>
#include <linux/slab.h>

static void exit_files(struct task_struct *task)
{
    unsigned int i;

    for (i = 0; i < NR_OPEN; i++) {
        if (task->files[i]) {
            fput(task->files[i]);
            task->files[i] = NULL;
        }
    }
}

static void notify_parent(struct task_struct *child)
{
    struct task_struct *parent;
    int sig;

    if (!child)
        return;

    parent = child->parent;
    if (!parent)
        return;

    sig = child->exit_signal;
    if (!sig)
        sig = SIGCHLD;
    signal_send(parent, sig);

    /*
     * waitpid() sleepers must wake even when SIGCHLD is blocked in the
     * parent's signal mask.
     */
    if (parent->state == TASK_SLEEPING)
        wake_up_process(parent);
}

void notify_parent_stop(struct task_struct *child)
{
    notify_parent(child);
}

void notify_parent_continue(struct task_struct *child)
{
    notify_parent(child);
}

void do_exit(long code)
{
    struct task_struct *task = current;

    task->exit_code = (int)code;
    task->wait_event = CHILD_EVENT_NONE;
    exit_files(task);
    /*
     * Keep task->mm until the zombie is reaped (free_task). Freeing page
     * tables here while TTBR0 still points at them corrupts the buddy
     * allocator (free_pages writes list headers into live table pages)
     * and breaks the next exec's maps.
     */
    task->state = TASK_ZOMBIE;
    dequeue_task(task);

    notify_parent(task);

    local_irq_enable();
    schedule();

    uart_puts("do_exit: schedule returned (bug)\n");
    for (;;)
        __asm__ volatile("wfi");
}

static struct task_struct *find_child(struct task_struct *parent, long pid,
                                      int state)
{
    struct list_head *pos;
    struct task_struct *child;
    struct task_struct *found = NULL;
    unsigned long flags;

    if (!parent)
        return NULL;

    task_list_lock_irqsave(&flags);
    for_each_task(pos, child) {
        if (child->parent != parent)
            continue;

        if (pid != -1 && child->pid != (pid_t)pid)
            continue;

        if (state != -1 && child->state != (enum task_state)state)
            continue;

        found = child;
        break;
    }
    task_list_unlock_irqrestore(flags);

    return found;
}

static void free_task(struct task_struct *task)
{
    if (!task)
        return;

    dequeue_task(task);
    task_detach(task);

    if (task->mm) {
        mm_put(task->mm);
        task->mm = NULL;
    }
    if (task->stack)
        free_pages(task->stack, 1);
    kfree(task);
}

void ksys_exit(long status)
{
    if (!current || !current->is_user)
        return;

    /* Linux wait status: (exit_code & 0xff) << 8 */
    do_exit((status & 0xff) << 8);
}

long ksys_wait4(long pid, int *status, long options)
{
    struct task_struct *parent = current;
    struct task_struct *child;

    if (!parent || !parent->is_user)
        return -EINVAL;

    if (pid < -1 || pid == 0)
        return -EINVAL;

    for (;;) {
        child = find_child(parent, pid, TASK_ZOMBIE);
        if (child) {
            long ret = (long)child->pid;

            if (status) {
                int code = child->exit_code;

                if (copy_to_user(status, &code, sizeof(code)))
                    return -EFAULT;
            }

            free_task(child);
            return ret;
        }

        if (options & WUNTRACED) {
            struct list_head *pos;
            unsigned long flags;

            task_list_lock_irqsave(&flags);
            for_each_task(pos, child) {
                int code;

                if (child->parent != parent)
                    continue;
                if (pid != -1 && child->pid != (pid_t)pid)
                    continue;
                if (child->wait_event != CHILD_EVENT_STOPPED)
                    continue;

                code = (child->stop_signal << 8) | 0x7f;
                task_list_unlock_irqrestore(flags);
                if (status &&
                    copy_to_user(status, &code, sizeof(code)))
                    return -EFAULT;
                child->wait_event = CHILD_EVENT_NONE;
                return (long)child->pid;
            }
            task_list_unlock_irqrestore(flags);
        }

        if (options & WCONTINUED) {
            struct list_head *pos;
            unsigned long flags;

            task_list_lock_irqsave(&flags);
            for_each_task(pos, child) {
                int code;

                if (child->parent != parent)
                    continue;
                if (pid != -1 && child->pid != (pid_t)pid)
                    continue;
                if (child->wait_event != CHILD_EVENT_CONTINUED)
                    continue;

                code = W_CONTINUED;
                task_list_unlock_irqrestore(flags);
                if (status &&
                    copy_to_user(status, &code, sizeof(code)))
                    return -EFAULT;
                child->wait_event = CHILD_EVENT_NONE;
                return (long)child->pid;
            }
            task_list_unlock_irqrestore(flags);
        }

        if (!find_child(parent, pid, -1))
            return -ECHILD;

        if (options & WNOHANG)
            return 0;

        sched_block(TASK_SLEEPING);
        local_irq_enable();
        schedule();
        local_irq_disable();
        if (!list_is_linked(&parent->run_list))
            enqueue_task(parent);

        if (signal_pending(parent))
            return -EINTR;
    }
}

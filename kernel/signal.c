/*
 * Minimal signals — kill(2) queues a pending bit; fatal signals terminate
 * the target on the next return-to-userspace path (do_signal).
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/stddef.h>

struct task_struct *find_task_by_pid(unsigned long pid)
{
    struct task_struct *walk;
    struct task_struct *start;

    if (!runqueue || pid == 0)
        return NULL;

    walk = start = runqueue;
    do {
        if (walk->pid == pid && walk->is_user &&
            walk->state != TASK_ZOMBIE && walk->state != TASK_DEAD)
            return walk;
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);

    return NULL;
}

static int signal_valid(int sig)
{
    return sig >= 0 && sig < _NSIG;
}

static void signal_queue(struct task_struct *task, int sig)
{
    if (!task || !signal_valid(sig) || sig == 0)
        return;

    task->pending |= (1UL << sig);

    if (task->state == TASK_SLEEPING)
        wake_up_process(task);
}

/*
 * Deliver pending fatal signals for current. May not return.
 */
void do_signal(struct pt_regs *regs)
{
    struct task_struct *task = current;
    unsigned long pending;
    int sig;

    (void)regs;

    if (!task || !task->is_user)
        return;

    pending = task->pending;
    if (!pending)
        return;

    if (pending & (1UL << SIGKILL))
        sig = SIGKILL;
    else if (pending & (1UL << SIGTERM))
        sig = SIGTERM;
    else {
        for (sig = 1; sig < (int)_NSIG_BPW; sig++) {
            if (pending & (1UL << sig))
                break;
        }
        if (sig >= (int)_NSIG_BPW)
            return;
    }

    task->pending = 0;
    ksys_exit(128 + sig);
}

long ksys_kill(long pid, int sig)
{
    struct task_struct *task;

    if (!signal_valid(sig))
        return -EINVAL;

    if (pid <= 0)
        return -EINVAL;

    task = find_task_by_pid((unsigned long)pid);
    if (!task)
        return -ESRCH;

    /* sig == 0: existence check only */
    if (sig == 0)
        return 0;

    signal_queue(task, sig);
    return 0;
}

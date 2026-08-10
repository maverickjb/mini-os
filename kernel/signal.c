/*
 * Minimal signals — kill(2) / rt_sigaction(2).
 *
 * Pending bits are delivered in do_signal() on return to userspace:
 *   SIG_DFL  → terminate (exit status 128+sig)
 *   SIG_IGN  → drop
 *   handler  → set ELR to handler, x0=sig, x30=old PC (simple return)
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
#include <linux/string.h>

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
    return sig >= 0 && sig < NSIG;
}

static void signal_queue(struct task_struct *task, int sig)
{
    if (!task || !signal_valid(sig) || sig == 0)
        return;

    task->pending |= (1UL << sig);

    if (task->state == TASK_SLEEPING)
        wake_up_process(task);
}

static int next_pending_signal(unsigned long pending)
{
    int sig;

    if (pending & (1UL << SIGKILL))
        return SIGKILL;
    if (pending & (1UL << SIGSTOP))
        return SIGSTOP;

    for (sig = 1; sig < (int)_NSIG_BPW; sig++) {
        if (pending & (1UL << sig))
            return sig;
    }
    return 0;
}

static void handle_signal(struct pt_regs *regs, int sig, sighandler_t handler)
{
    if (!regs || !handler || handler == SIG_DFL || handler == SIG_IGN)
        return;

    /*
     * Minimal delivery without a full sigframe: pass sig in x0 and set
     * LR to the interrupted PC so a plain `ret` resumes user code.
     */
    regs->x0 = (unsigned long)sig;
    regs->x30 = regs->elr_el1;
    regs->elr_el1 = (unsigned long)handler;
}

/*
 * Deliver one pending signal for current. May not return (default terminate).
 */
void do_signal(struct pt_regs *regs)
{
    struct task_struct *task = current;
    int sig;
    sighandler_t handler;

    if (!task || !task->is_user || !regs)
        return;

    for (;;) {
        sig = next_pending_signal(task->pending);
        if (!sig)
            return;

        task->pending &= ~(1UL << sig);

        if (sig == SIGKILL) {
            ksys_exit(128 + sig);
            return;
        }

        handler = task->actions[sig].sa_handler;

        if (handler == SIG_IGN)
            continue;

        if (handler == SIG_DFL) {
            /* Default: terminate (ignore stop/cont for now). */
            if (sig == SIGCHLD || sig == SIGURG || sig == SIGWINCH)
                continue;
            ksys_exit(128 + sig);
            return;
        }

        if (task->actions[sig].sa_flags & SA_RESETHAND)
            task->actions[sig].sa_handler = SIG_DFL;

        handle_signal(regs, sig, handler);
        return;
    }
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

long ksys_rt_sigaction(int sig, const struct sigaction *act,
                       struct sigaction *oldact, unsigned long sigsetsize)
{
    struct task_struct *task = current;
    struct sigaction old;
    struct sigaction new;

    if (!task || !task->is_user)
        return -EINVAL;

    if (sigsetsize != sizeof(sigset_t))
        return -EINVAL;

    if (sig <= 0 || sig >= NSIG)
        return -EINVAL;

    if (sig == SIGKILL || sig == SIGSTOP)
        return -EINVAL;

    old = task->actions[sig];

    if (oldact) {
        if (copy_to_user(oldact, &old, sizeof(old)))
            return -EFAULT;
    }

    if (act) {
        if (copy_from_user(&new, act, sizeof(new)))
            return -EFAULT;

        /* sa_mask is stored but not yet applied during delivery. */
        task->actions[sig] = new;
    }

    return 0;
}

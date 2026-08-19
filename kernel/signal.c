/*
 * Minimal signals — kill / rt_sigaction / rt_sigprocmask / rt_sigpending /
 * rt_sigreturn.
 *
 * Pending bits are delivered in do_signal() on return to userspace:
 *   SIG_DFL  → terminate (exit status 128+sig)
 *   SIG_IGN  → drop
 *   handler  → user stack frame, ELR=handler, LR=sa_restorer (rt_sigreturn)
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/mm.h>

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

    task->pending |= SIG_BIT(sig);

    if (task->state == TASK_SLEEPING && signal_pending(task))
        wake_up_process(task);
}

int signal_pending(struct task_struct *task)
{
    unsigned long blocked;
    unsigned long ready;

    if (!task)
        return 0;

    blocked = task->blocked & ~SIG_UNBLOCKABLE;
    ready = task->pending & ~blocked;
    return ready != 0;
}

static int next_pending_signal(unsigned long pending, unsigned long blocked)
{
    unsigned long ready;
    int sig;

    blocked &= ~SIG_UNBLOCKABLE;
    ready = pending & ~blocked;

    if (ready & SIG_BIT(SIGKILL))
        return SIGKILL;
    if (ready & SIG_BIT(SIGSTOP))
        return SIGSTOP;

    for (sig = 1; sig < (int)_NSIG_BPW; sig++) {
        if (ready & SIG_BIT(sig))
            return sig;
    }
    return 0;
}

static unsigned long read_user_sp(void)
{
    unsigned long sp;

    __asm__ volatile("mrs %0, sp_el0" : "=r"(sp));
    return sp;
}

static void write_user_sp(unsigned long sp)
{
    __asm__ volatile("msr sp_el0, %0" : : "r"(sp));
    if (current)
        current->user_sp = sp;
}

static int signal_frame_ok(unsigned long sp)
{
    unsigned long top;
    unsigned long bot;

    if (!sp || (sp & 15UL))
        return 0;
    if (sp > ~0UL - sizeof(struct signal_frame))
        return 0;

    if (!current || !current->mm || !current->mm->stack_top)
        return 0;

    top = current->mm->stack_top;
    bot = top - PAGE_SIZE;
    if (sp < bot || sp + sizeof(struct signal_frame) > top)
        return 0;

    return 1;
}

static int setup_signal_frame(struct pt_regs *regs, int sig,
                              sighandler_t handler, void (*restorer)(void),
                              unsigned long old_blocked)
{
    struct signal_frame frame;
    unsigned long sp;

    if (!restorer)
        return -EINVAL;

    sp = read_user_sp();
    if (sp < sizeof(frame) || (sp & 15UL))
        return -EFAULT;

    copy_pt_regs(&frame.regs, regs);
    frame.user_sp = sp;
    frame.blocked = old_blocked;

    sp -= sizeof(frame);
    sp &= ~15UL;

    if (!signal_frame_ok(sp))
        return -EFAULT;

    if (copy_to_user((void *)sp, &frame, sizeof(frame)))
        return -EFAULT;

    write_user_sp(sp);

    regs->x0 = (unsigned long)sig;
    regs->x30 = (unsigned long)restorer;
    regs->elr_el1 = (unsigned long)handler;
    return 0;
}

static void handle_signal(struct pt_regs *regs, int sig,
                          struct sigaction *ka)
{
    unsigned long old_blocked;
    unsigned long new_blocked;

    if (!regs || !ka || !ka->sa_handler ||
        ka->sa_handler == SIG_DFL || ka->sa_handler == SIG_IGN)
        return;

    old_blocked = current->blocked;
    new_blocked = old_blocked | ka->sa_mask.sig[0];
    if (!(ka->sa_flags & SA_NODEFER))
        new_blocked |= SIG_BIT(sig);
    new_blocked &= ~SIG_UNBLOCKABLE;

    if (setup_signal_frame(regs, sig, ka->sa_handler, ka->sa_restorer,
                           old_blocked) < 0)
        ksys_exit(128 + SIGSEGV);

    current->blocked = new_blocked;
}

/*
 * Deliver one pending signal for current. May not return (default terminate).
 */
void do_signal(struct pt_regs *regs)
{
    struct task_struct *task = current;
    struct sigaction ka;
    int sig;
    sighandler_t handler;

    if (!task || !task->is_user || !regs)
        return;

    for (;;) {
        sig = next_pending_signal(task->pending, task->blocked);
        if (!sig)
            return;

        task->pending &= ~SIG_BIT(sig);

        if (sig == SIGKILL) {
            ksys_exit(128 + sig);
            return;
        }

        ka = task->actions[sig];
        handler = ka.sa_handler;

        if (handler == SIG_IGN)
            continue;

        if (handler == SIG_DFL) {
            /* Default: terminate (ignore stop/cont for now). */
            if (sig == SIGCHLD || sig == SIGURG || sig == SIGWINCH)
                continue;
            ksys_exit(128 + sig);
            return;
        }

        if (ka.sa_flags & SA_RESETHAND)
            task->actions[sig].sa_handler = SIG_DFL;

        handle_signal(regs, sig, &ka);
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

        new.sa_mask.sig[0] &= ~SIG_UNBLOCKABLE;
        task->actions[sig] = new;
    }

    return 0;
}

/*
 * Restore the interrupted context from the user-stack signal frame.
 * Returns 0 on success. The return value is not a user-visible syscall
 * result — caller must not write it to regs->x0.
 */
long ksys_rt_sigreturn(struct pt_regs *regs)
{
    struct signal_frame frame;
    unsigned long sp;

    if (!regs)
        return -EINVAL;

    sp = read_user_sp();
    if (!signal_frame_ok(sp))
        return -EFAULT;

    if (copy_from_user(&frame, (void *)sp, sizeof(frame)))
        return -EFAULT;

    copy_pt_regs(regs, &frame.regs);
    write_user_sp(frame.user_sp);
    if (current)
        current->blocked = frame.blocked & ~SIG_UNBLOCKABLE;
    return 0;
}

long ksys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset,
                         unsigned long sigsetsize)
{
    struct task_struct *task = current;
    sigset_t old;
    sigset_t newset;
    unsigned long newmask;

    if (!task || !task->is_user)
        return -EINVAL;

    if (sigsetsize != sizeof(sigset_t))
        return -EINVAL;

    old.sig[0] = task->blocked;

    if (set) {
        if (copy_from_user(&newset, set, sizeof(newset)))
            return -EFAULT;

        newset.sig[0] &= ~SIG_UNBLOCKABLE;

        switch (how) {
        case SIG_BLOCK:
            newmask = old.sig[0] | newset.sig[0];
            break;
        case SIG_UNBLOCK:
            newmask = old.sig[0] & ~newset.sig[0];
            break;
        case SIG_SETMASK:
            newmask = newset.sig[0];
            break;
        default:
            return -EINVAL;
        }

        task->blocked = newmask;
    }

    if (oldset) {
        if (copy_to_user(oldset, &old, sizeof(old)))
            return -EFAULT;
    }

    return 0;
}

long ksys_rt_sigpending(sigset_t *set, unsigned long sigsetsize)
{
    struct task_struct *task = current;
    sigset_t pending;
    unsigned long blocked;

    if (!task || !task->is_user)
        return -EINVAL;

    if (sigsetsize != sizeof(sigset_t))
        return -EINVAL;

    if (!set)
        return -EFAULT;

    blocked = task->blocked & ~SIG_UNBLOCKABLE;
    pending.sig[0] = task->pending & ~blocked;

    if (copy_to_user(set, &pending, sizeof(pending)))
        return -EFAULT;

    return 0;
}
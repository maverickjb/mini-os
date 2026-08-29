/*
 * Minimal signals — kill / rt_sigaction / rt_sigprocmask / rt_sigpending /
 * rt_sigsuspend / rt_sigreturn.
 *
 * Pending bits are delivered in do_signal() on return to userspace:
 *   SIGSTOP  → TASK_STOPPED (not a zombie); cannot catch/block
 *   SIGTSTP / SIGTTIN / SIGTTOU → stop if SIG_DFL; else handler or ignore
 *   SIGCONT  → resume TASK_STOPPED; drops pending stop signals
 *   SIG_DFL  → terminate (wait status = signal number), except ignored
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
#include <linux/pid.h>
#include <linux/reboot.h>
#include <asm/irqflags.h>

struct task_struct *find_task_by_pid(pid_t pid)
{
    struct task_struct *walk;
    struct task_struct *start;

    if (!runqueue || pid <= 0)
        return NULL;

    walk = start = runqueue;
    do {
        if (walk->pid == pid && walk->is_user &&
            walk->state != TASK_DEAD)
            return walk;
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);

    return NULL;
}

static int signal_valid(int sig)
{
    return sig >= 0 && sig < NSIG;
}

static int sig_kernel_stop(int sig)
{
    return sig == SIGSTOP || sig == SIGTSTP ||
           sig == SIGTTIN || sig == SIGTTOU;
}

#define SIG_STOP_MASK (SIG_BIT(SIGSTOP) | SIG_BIT(SIGTSTP) | \
                       SIG_BIT(SIGTTIN) | SIG_BIT(SIGTTOU))

static void signal_queue(struct task_struct *task, int sig)
{
    if (!task || !signal_valid(sig) || sig == 0)
        return;

    task->pending |= SIG_BIT(sig);

    /* Stop and continue cancel each other, like Linux. */
    if (sig == SIGCONT)
        task->pending &= ~SIG_STOP_MASK;
    else if (sig_kernel_stop(sig))
        task->pending &= ~SIG_BIT(SIGCONT);

    if (task->state == TASK_STOPPED && sig == SIGCONT) {
        task->state = TASK_RUNNING;
        task->wait_event = CHILD_EVENT_CONTINUED;
        notify_parent_continue(task);
        wake_up_process(task);
    } else if (task->state == TASK_SLEEPING && signal_pending(task)) {
        wake_up_process(task);
    } else if (task->state == TASK_STOPPED && sig == SIGKILL) {
        wake_up_process(task);
    }
}

void signal_send(struct task_struct *task, int sig)
{
    signal_queue(task, sig);
}

static int signal_one_process(pid_t pid, int sig)
{
    struct task_struct *task;

    task = find_task_by_pid(pid);
    if (!task)
        return -ESRCH;

    if (sig == 0)
        return 0;

    signal_send(task, sig);
    return 0;
}

static int signal_process_group(pid_t pgid, int sig)
{
    struct task_struct *task;
    int found = 0;

    if (pgid <= 0)
        return -ESRCH;

    for (task = runqueue; task; task = task->next) {
        if (!task->is_user ||
            task->state == TASK_ZOMBIE || task->state == TASK_DEAD)
            continue;
        if (task->pgid != pgid)
            continue;

        found = 1;
        if (sig != 0)
            signal_send(task, sig);
    }

    return found ? 0 : -ESRCH;
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

/*
 * Park current in TASK_STOPPED until SIGCONT or SIGKILL. The task stays
 * on the runqueue so waitpid can still see it; pick_next_task skips it.
 */
static void do_signal_stop(int sig)
{
    struct task_struct *task = current;

    task->stop_signal = sig;
    task->wait_event = CHILD_EVENT_STOPPED;
    task->state = TASK_STOPPED;
    notify_parent_stop(task);
    local_irq_enable();
    schedule();
    local_irq_disable();
    if (task->state != TASK_RUNNING)
        task->state = TASK_RUNNING;
    task->time_slice = SCHED_TIME_SLICE;
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
    if (current->restore_sigmask) {
        old_blocked = current->saved_blocked;
        current->restore_sigmask = 0;
    }
    new_blocked = current->blocked | ka->sa_mask.sig[0];
    if (!(ka->sa_flags & SA_NODEFER))
        new_blocked |= SIG_BIT(sig);
    new_blocked &= ~SIG_UNBLOCKABLE;

    if (setup_signal_frame(regs, sig, ka->sa_handler, ka->sa_restorer,
                           old_blocked) < 0)
        do_exit(SIGSEGV);

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
            do_exit(SIGKILL);
            return;
        }

        if (sig == SIGSTOP) {
            do_signal_stop(SIGSTOP);
            continue;
        }

        ka = task->actions[sig];
        handler = ka.sa_handler;

        if (handler == SIG_IGN)
            continue;

        if (handler == SIG_DFL) {
            if (sig_kernel_stop(sig)) {
                do_signal_stop(sig);
                continue;
            }
            if (task->pid == 1 &&
                (sig == SIGUSR1 || sig == SIGUSR2 || sig == SIGTERM)) {
                kernel_init_shutdown(sig);
                return;
            }
            /* Default: terminate, except job-control / ignored signals. */
            if (sig == SIGCHLD || sig == SIGCONT ||
                sig == SIGURG || sig == SIGWINCH)
                continue;
            do_exit(sig & 0x7f);
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
    if (!signal_valid(sig))
        return -EINVAL;

    if (pid > 0)
        return signal_one_process((pid_t)pid, sig);
    if (pid == 0) {
        if (!current)
            return -ESRCH;
        return signal_process_group(current->pgid, sig);
    }
    if (pid < -1)
        return signal_process_group((pid_t)(-pid), sig);

    /* pid == -1: all user processes except the caller. */
    {
        struct task_struct *task;
        int found = 0;

        for (task = runqueue; task; task = task->next) {
            if (!task->is_user || task == current ||
                task->state == TASK_DEAD)
                continue;
            found = 1;
            if (sig != 0)
                signal_send(task, sig);
        }
        return found ? 0 : -ESRCH;
    }
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

long ksys_rt_sigsuspend(const sigset_t *unewset, unsigned long sigsetsize)
{
    struct task_struct *task = current;
    sigset_t newset;

    if (!task || !task->is_user)
        return -EINVAL;

    if (sigsetsize != sizeof(sigset_t))
        return -EINVAL;

    if (!unewset)
        return -EFAULT;

    if (copy_from_user(&newset, unewset, sizeof(newset)))
        return -EFAULT;

    task->saved_blocked = task->blocked;
    task->restore_sigmask = 1;
    task->blocked = newset.sig[0] & ~SIG_UNBLOCKABLE;

    while (!signal_pending(task)) {
        task->state = TASK_SLEEPING;
        local_irq_enable();
        schedule();
        local_irq_disable();
        task->state = TASK_RUNNING;
        task->time_slice = SCHED_TIME_SLICE;
    }

    return -EINTR;
}
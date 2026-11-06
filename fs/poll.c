/*
 * ppoll(2) — wait for TTY / pipe / file readiness (BusyBox ash line editing).
 */

#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/tick.h>
#include <linux/timekeeping.h>
#include <linux/stat.h>
#include <asm/irqflags.h>

void poll_table_init(struct poll_table *pt)
{
    unsigned int i;

    if (!pt)
        return;

    pt->n = 0;
    for (i = 0; i < NR_OPEN; i++) {
        pt->waiters[i].head = NULL;
        pt->waiters[i].wait.task = current;
        pt->waiters[i].wait.next = NULL;
    }
}

void poll_wait(struct wait_queue_head *wq, struct poll_table *pt)
{
    struct poll_waiter *pw;

    if (!wq || !pt || pt->n >= NR_OPEN)
        return;

    pw = &pt->waiters[pt->n];
    pw->head = wq;
    pw->wait.task = current;
    pw->wait.next = NULL;
    add_wait_queue(wq, &pw->wait);
    pt->n++;
}

void poll_table_finish(struct poll_table *pt)
{
    unsigned int i;

    if (!pt)
        return;

    for (i = 0; i < pt->n; i++) {
        if (pt->waiters[i].head)
            remove_wait_queue(pt->waiters[i].head, &pt->waiters[i].wait);
        pt->waiters[i].head = NULL;
        pt->waiters[i].wait.next = NULL;
    }
    pt->n = 0;
}

static unsigned int vfs_poll(struct file *file, struct poll_table *pt)
{
    if (!file)
        return POLLNVAL;

    if (file->f_op && file->f_op->poll)
        return file->f_op->poll(file, pt);

    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

static unsigned int do_poll_scan(struct pollfd *fds, unsigned int nfds,
                                 struct poll_table *pt)
{
    struct task_struct *task = current;
    unsigned int i;
    unsigned int nready = 0;

    for (i = 0; i < nfds; i++) {
        unsigned int mask;
        unsigned int want;
        struct file *file;
        int fd = fds[i].fd;

        fds[i].revents = 0;
        if (fd < 0)
            continue;

        if ((unsigned int)fd >= NR_OPEN || !task->files[fd]) {
            fds[i].revents = POLLNVAL;
            nready++;
            continue;
        }

        file = task->files[fd];
        mask = vfs_poll(file, pt);
        want = (unsigned int)fds[i].events | POLLERR | POLLHUP | POLLNVAL;
        fds[i].revents = (short)(mask & want);
        if (fds[i].revents)
            nready++;
    }

    return nready;
}

static unsigned long timespec_to_jiffies(const struct timespec *ts)
{
    unsigned long j;

    j = (unsigned long)ts->tv_sec * HZ;
    j += ((unsigned long)ts->tv_nsec * HZ + NSEC_PER_SEC - 1) / NSEC_PER_SEC;
    return j;
}

static int timespec_valid(const struct timespec *ts)
{
    if (!ts)
        return 0;
    if (ts->tv_sec < 0)
        return 0;
    if (ts->tv_nsec < 0 || ts->tv_nsec >= NSEC_PER_SEC)
        return 0;
    return 1;
}

long ksys_ppoll(struct pollfd *ufds, unsigned int nfds,
                const struct timespec *tsp, const sigset_t *sigmask,
                unsigned long sigsetsize)
{
    struct pollfd pfds[NR_OPEN];
    struct poll_table pt;
    struct timespec tmo;
    struct task_struct *task = current;
    unsigned long deadline = 0;
    unsigned int nready;
    int timed;
    int applied_mask = 0;

    if (!task || !task->is_user)
        return -EINVAL;

    if (nfds > NR_OPEN)
        return -EINVAL;

    if (nfds && !ufds)
        return -EFAULT;

    if (nfds) {
        if (copy_from_user(pfds, ufds, nfds * sizeof(struct pollfd)))
            return -EFAULT;
    }

    if (tsp) {
        if (copy_from_user(&tmo, tsp, sizeof(tmo)))
            return -EFAULT;
        if (!timespec_valid(&tmo))
            return -EINVAL;
        if (tmo.tv_sec || tmo.tv_nsec) {
            unsigned long delta = timespec_to_jiffies(&tmo);

            if (!delta)
                delta = 1;
            deadline = get_jiffies() + delta;
        }
    }

    if (sigmask) {
        sigset_t newset;

        if (sigsetsize != sizeof(sigset_t))
            return -EINVAL;
        if (copy_from_user(&newset, sigmask, sizeof(newset)))
            return -EFAULT;

        task->saved_blocked = task->blocked;
        task->restore_sigmask = 1;
        task->blocked = newset.sig[0] & ~SIG_UNBLOCKABLE;
        applied_mask = 1;
    }

    timed = (tsp && !deadline);
    poll_table_init(&pt);

    for (;;) {
        nready = do_poll_scan(pfds, nfds, timed ? NULL : &pt);
        if (nready || timed)
            break;

        if (deadline && get_jiffies() >= deadline) {
            nready = 0;
            break;
        }

        if (signal_pending(task)) {
            poll_table_finish(&pt);
            return -EINTR;
        }

        local_irq_disable();
        sched_block(TASK_SLEEPING);
        if (deadline)
            task->wake_jiffies = deadline;

        nready = do_poll_scan(pfds, nfds, NULL);
        if (nready || signal_pending(task) ||
            (deadline && get_jiffies() >= deadline)) {
            task->wake_jiffies = 0;
            if (!list_is_linked(&task->run_list))
                enqueue_task(task);
            local_irq_enable();
            if (nready)
                break;
            poll_table_finish(&pt);
            if (signal_pending(task))
                return -EINTR;
            nready = 0;
            break;
        }

        local_irq_enable();
        schedule();
        local_irq_disable();
        task->wake_jiffies = 0;
        if (!list_is_linked(&task->run_list))
            enqueue_task(task);
        local_irq_enable();
        poll_table_finish(&pt);
        poll_table_init(&pt);

        if (signal_pending(task))
            return -EINTR;
        if (deadline && get_jiffies() >= deadline) {
            nready = 0;
            break;
        }
    }

    poll_table_finish(&pt);

    if (nfds && copy_to_user(ufds, pfds, nfds * sizeof(struct pollfd)))
        return -EFAULT;

    if (applied_mask) {
        task->blocked = task->saved_blocked & ~SIG_UNBLOCKABLE;
        task->restore_sigmask = 0;
    }

    return (long)nready;
}

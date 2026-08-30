#include <linux/pid.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>

long ksys_getpid(void)
{
    if (!current)
        return -EINVAL;
    return (long)current->tgid;
}

long ksys_getpgrp(void)
{
    if (!current)
        return -ESRCH;

    return (long)current->pgid;
}

static int pgid_exists(pid_t pgid)
{
    struct task_struct *walk;
    struct task_struct *start;
    int exists = 0;
    unsigned long flags;

    if (!runqueue || pgid <= 0)
        return 0;

    spin_lock_irqsave(&runqueue_lock, flags);
    walk = start = runqueue;
    do {
        if (walk->is_user &&
            walk->state != TASK_ZOMBIE && walk->state != TASK_DEAD &&
            walk->pgid == pgid) {
            exists = 1;
            break;
        }
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);
    spin_unlock_irqrestore(&runqueue_lock, flags);

    return exists;
}

long ksys_setpgid(pid_t pid, pid_t pgid)
{
    struct task_struct *task;

    if (!current || !current->is_user)
        return -EINVAL;

    if (pid == 0)
        pid = current->pid;

    task = find_task_by_pid(pid);
    if (!task)
        return -ESRCH;

    if (pgid == 0)
        pgid = pid;

    if (pgid < 0)
        return -EINVAL;

    /*
     * Simplified mini-OS rule:
     * only current process or its child.
     */
    if (task != current && task->parent != current)
        return -ESRCH;

    /*
     * Create a new group (pgid == pid) or join an existing one
     * (some live task already has that pgid), e.g. setpgid(102, 101).
     */
    if (pgid != pid && !pgid_exists(pgid))
        return -EPERM;

    /* Session leader cannot change process group. */
    if (task->sid == task->pid)
        return -EPERM;

    /* Can only join a group in the same session. */
    if (task->sid != current->sid)
        return -EPERM;

    task->pgid = pgid;

    return 0;
}

long ksys_getsid(pid_t pid)
{
    struct task_struct *task;

    if (!current || !current->is_user)
        return -ESRCH;

    if (pid == 0)
        return (long)current->sid;

    task = find_task_by_pid(pid);
    if (!task)
        return -ESRCH;

    return (long)task->sid;
}

long ksys_setsid(void)
{
    struct task_struct *task = current;

    if (!task || !task->is_user)
        return -EINVAL;

    /*
     * Fail if this pid is already a process-group id (session leader
     * or group leader). Same rule as Linux.
     */
    if (pgid_exists(task->pid))
        return -EPERM;

    task->sid = task->pid;
    task->pgid = task->pid;
    return (long)task->sid;
}

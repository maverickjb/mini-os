#include <linux/pid.h>
#include <linux/sched/task.h>
#include <linux/signal.h>
#include <linux/errno.h>

long ksys_getpid(void)
{
    if (!current)
        return -EINVAL;
    return (long)current->pid;
}

long ksys_getpgrp(void)
{
    if (!current)
        return -ESRCH;

    return (long)current->pgid;
}

static int pgid_exists(unsigned long pgid)
{
    struct task_struct *walk;
    struct task_struct *start;

    if (!runqueue || pgid == 0)
        return 0;

    walk = start = runqueue;
    do {
        if (walk->is_user &&
            walk->state != TASK_ZOMBIE && walk->state != TASK_DEAD &&
            walk->pgid == pgid)
            return 1;
        walk = walk->next ? walk->next : runqueue;
    } while (walk != start);

    return 0;
}

long ksys_setpgid(pid_t pid, pid_t pgid)
{
    struct task_struct *task;

    if (!current || !current->is_user)
        return -EINVAL;

    if (pid == 0)
        pid = (pid_t)current->pid;

    task = find_task_by_pid((unsigned long)pid);
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
    if (pgid != pid && !pgid_exists((unsigned long)pgid))
        return -EPERM;

    task->pgid = (unsigned long)pgid;

    return 0;
}

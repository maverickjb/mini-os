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
     * First version: only allow creating a group
     * whose PGID equals the target PID.
     */
    if (pgid != pid)
        return -EPERM;

    task->pgid = (unsigned long)pgid;

    return 0;
}

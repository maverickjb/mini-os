#include <linux/pid.h>
#include <linux/sched/task.h>
#include <linux/errno.h>

long ksys_getpid(void)
{
    if (!current)
        return -EINVAL;
    return (long)current->pid;
}

/*
 * Timer syscalls — nanosleep and related helpers.
 */

#include <linux/tick.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <asm/irqflags.h>

#define NSEC_PER_SEC 1000000000L

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

static unsigned long timespec_to_jiffies(const struct timespec *ts)
{
    unsigned long j;

    j = (unsigned long)ts->tv_sec * HZ;
    j += ((unsigned long)ts->tv_nsec * HZ + NSEC_PER_SEC - 1) / NSEC_PER_SEC;
    return j;
}

long ksys_nanosleep(const struct timespec *req, struct timespec *rem)
{
    struct timespec t;
    struct task_struct *task = current;
    unsigned long delta;
    unsigned long deadline;

    (void)rem;

    if (!task || !task->is_user || !req)
        return -EINVAL;

    if (copy_from_user(&t, req, sizeof(t)))
        return -EFAULT;

    if (!timespec_valid(&t))
        return -EINVAL;

    if (t.tv_sec == 0 && t.tv_nsec == 0)
        return 0;

    delta = timespec_to_jiffies(&t);
    if (delta == 0)
        delta = 1;

    deadline = get_jiffies() + delta;

    task->wake_jiffies = deadline;
    task->state = TASK_SLEEPING;
    local_irq_enable();
    schedule();
    local_irq_disable();

    task->wake_jiffies = 0;
    task->state = TASK_RUNNING;

    return 0;
}

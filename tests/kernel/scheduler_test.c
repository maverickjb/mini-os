#include "test.h"

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/gfp.h>
#include <linux/string.h>

int test_scheduler(void)
{
    struct task_struct *task;
    struct task_struct *next;
    struct task_struct *prev = get_current();

    task = alloc_pages(0);
    EXPECT_TRUE(task != NULL);

    memset(task, 0, sizeof(*task));
    task->pid = 9999;
    task->state = TASK_RUNNING;
    task->is_user = 0;

    enqueue_task(task);
    EXPECT_TRUE(runqueue != NULL);

    next = pick_next_task(prev);
    EXPECT_TRUE(next == task);

    dequeue_task(task);
    free_pages(task, 0);

    return 0;
}

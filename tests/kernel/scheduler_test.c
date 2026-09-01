#include "test.h"

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/irqflags.h>

int test_scheduler(void)
{
    struct task_struct *task;
    struct task_struct *next;
    struct task_struct *prev = get_current();
    unsigned long flags;

    task = kmalloc(sizeof(*task));
    EXPECT_TRUE(task != NULL);

    memset(task, 0, sizeof(*task));
    INIT_LIST_HEAD(&task->run_list);
    task->pid = 9999;
    task->state = TASK_RUNNING;
    task->is_user = 0;

    local_irq_disable();

    enqueue_task(task);
    EXPECT_TRUE(cpu_rq.nr_running > 0);
    EXPECT_TRUE(!list_empty(&cpu_rq.tasks));

    spin_lock_irqsave(&cpu_rq.lock, flags);
    next = pick_next_task(prev);
    spin_unlock_irqrestore(&cpu_rq.lock, flags);
    EXPECT_TRUE(next == task);

    dequeue_task(task);
    EXPECT_TRUE(cpu_rq.nr_running == 0);

    local_irq_enable();

    kfree(task);

    return 0;
}

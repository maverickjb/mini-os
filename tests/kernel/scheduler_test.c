#include "test.h"

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/irqflags.h>
#include <asm/smp.h>

int test_scheduler(void)
{
    struct task_struct *task;
    struct task_struct *next;
    struct task_struct *prev = get_current();
    struct rq *rq = &cpu_data[smp_processor_id()].rq;
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
    EXPECT_TRUE(rq->nr_running > 0);
    EXPECT_TRUE(!list_empty(&rq->tasks));

    spin_lock_irqsave(&rq->lock, flags);
    next = pick_next_task(rq, prev);
    spin_unlock_irqrestore(&rq->lock, flags);
    EXPECT_TRUE(next == task);

    dequeue_task(task);
    EXPECT_TRUE(rq->nr_running == 0);

    local_irq_enable();

    kfree(task);

    return 0;
}

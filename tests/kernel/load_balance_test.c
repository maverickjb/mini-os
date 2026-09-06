/*
 * Pull-based load balance: empty CPU steals from a busier runqueue.
 *
 * Stub tasks have no kernel stack; schedule() will not switch to them.
 */

#include "test.h"

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/smp.h>

static struct task_struct *make_stub(pid_t pid)
{
    struct task_struct *task;

    task = kmalloc(sizeof(*task));
    if (!task)
        return NULL;

    memset(task, 0, sizeof(*task));
    INIT_LIST_HEAD(&task->run_list);
    INIT_LIST_HEAD(&task->task_list);
    task->pid = pid;
    task->state = TASK_SLEEPING;
    task->cpu = 0;
    task->is_user = 0;
    /* stack == NULL → schedule() must not context_switch to this stub */

    return task;
}

static void drain_rq(struct rq *rq)
{
    struct list_head *pos;
    struct task_struct *t;

    while (!list_empty(&rq->tasks)) {
        pos = rq->tasks.next;
        t = list_entry(pos, struct task_struct, run_list);
        dequeue_task(t);
    }
}

int test_load_balance(void)
{
    struct task_struct *a, *b, *c;
    struct rq *rq0 = &cpu_data[0].rq;
    struct rq *rq1 = &cpu_data[1].rq;
    int pulled;

    a = make_stub(9001);
    b = make_stub(9002);
    c = make_stub(9003);
    EXPECT_TRUE(a && b && c);

    drain_rq(rq0);
    drain_rq(rq1);

    /* No busier CPU than ourselves when we hold all the load. */
    enqueue_task_cpu(a, 0);
    enqueue_task_cpu(b, 0);
    enqueue_task_cpu(c, 0);
    EXPECT_EQ((int)rq0->nr_running, 3);
    EXPECT_TRUE(try_pull_task(0) == 0);
    EXPECT_EQ((int)rq0->nr_running, 3);

    /* Idle CPU1 pulls one task: 3 > 0+1 */
    pulled = try_pull_task(1);
    EXPECT_TRUE(pulled == 1);
    EXPECT_EQ((int)rq0->nr_running, 2);
    EXPECT_EQ((int)rq1->nr_running, 1);

    /* 2 vs 1: not worth another pull (need source > dest+1) */
    pulled = try_pull_task(1);
    EXPECT_TRUE(pulled == 0);
    EXPECT_EQ((int)rq0->nr_running, 2);
    EXPECT_EQ((int)rq1->nr_running, 1);

    drain_rq(rq0);
    drain_rq(rq1);

    /* Never steal the last runnable task */
    enqueue_task_cpu(a, 0);
    EXPECT_EQ((int)rq0->nr_running, 1);
    pulled = try_pull_task(1);
    EXPECT_TRUE(pulled == 0);
    EXPECT_EQ((int)rq0->nr_running, 1);
    EXPECT_EQ((int)rq1->nr_running, 0);

    drain_rq(rq0);
    drain_rq(rq1);

    kfree(a);
    kfree(b);
    kfree(c);

    return 0;
}

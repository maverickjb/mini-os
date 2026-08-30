#include "test.h"

#include <linux/wait.h>
#include <linux/sched/task.h>

static int cond_true(void)
{
    return 1;
}

int test_waitqueue(void)
{
    struct wait_queue_head wq;
    DECLARE_WAITQUEUE(entry, current);

    init_waitqueue_head(&wq);

    add_wait_queue(&wq, &entry);
    remove_wait_queue(&wq, &entry);

    EXPECT_EQ(wait_event(&wq, cond_true), 0);

    wake_up(&wq);

    return 0;
}

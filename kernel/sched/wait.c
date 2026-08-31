/*
 * Wait queues — Linux-style sleep/wake on a linked list of waiters.
 */

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <asm/irqflags.h>

static int waitqueue_contains(struct wait_queue_head *wq,
                              struct wait_queue_entry *entry)
{
    struct wait_queue_entry *walk;

    for (walk = wq->head; walk; walk = walk->next) {
        if (walk == entry)
            return 1;
    }
    return 0;
}

void init_waitqueue_head(struct wait_queue_head *wq)
{
    wq->head = NULL;
    spin_lock_init(&wq->lock);
}

void add_wait_queue(struct wait_queue_head *wq,
                    struct wait_queue_entry *entry)
{
    spin_lock(&wq->lock);

    entry->next = wq->head;
    wq->head = entry;

    spin_unlock(&wq->lock);
}

void remove_wait_queue(struct wait_queue_head *wq,
                       struct wait_queue_entry *entry)
{
    struct wait_queue_entry **p;

    spin_lock(&wq->lock);

    p = &wq->head;
    while (*p) {
        if (*p == entry) {
            *p = entry->next;
            entry->next = NULL;
            break;
        }
        p = &(*p)->next;
    }

    spin_unlock(&wq->lock);
}

void prepare_to_wait(struct wait_queue_head *wq,
                     struct wait_queue_entry *entry)
{
    spin_lock(&wq->lock);

    if (!entry->task)
        entry->task = current;

    if (!waitqueue_contains(wq, entry)) {
        entry->next = wq->head;
        wq->head = entry;
    }

    current->state = TASK_SLEEPING;
    dequeue_task(current);

    spin_unlock(&wq->lock);
}

void finish_wait(struct wait_queue_head *wq,
                 struct wait_queue_entry *entry)
{
    remove_wait_queue(wq, entry);
    if (!list_is_linked(&current->run_list))
        enqueue_task(current);
    else
        current->state = TASK_RUNNING;
}

void wake_up(struct wait_queue_head *wq)
{
    struct wait_queue_entry *entry;

    spin_lock(&wq->lock);

    for (entry = wq->head; entry; entry = entry->next) {
        if (entry->task && entry->task->state == TASK_SLEEPING)
            wake_up_process(entry->task);
    }

    spin_unlock(&wq->lock);
}

int wait_event(struct wait_queue_head *wq, int (*condition)(void))
{
    struct wait_queue_entry wait;

    wait.task = current;
    wait.next = NULL;

    for (;;) {
        local_irq_disable();

        if (condition()) {
            local_irq_enable();
            break;
        }

        prepare_to_wait(wq, &wait);

        local_irq_enable();

        schedule();

        finish_wait(wq, &wait);
    }

    return 0;
}

long wait_event_interruptible(struct wait_queue_head *wq,
                              int (*condition)(void))
{
    struct wait_queue_entry wait;

    wait.task = current;
    wait.next = NULL;

    for (;;) {
        local_irq_disable();

        if (condition()) {
            local_irq_enable();
            break;
        }

        if (signal_pending(current)) {
            local_irq_enable();
            return -EINTR;
        }

        prepare_to_wait(wq, &wait);

        local_irq_enable();

        schedule();

        finish_wait(wq, &wait);

        if (signal_pending(current))
            return -EINTR;
    }

    return 0;
}

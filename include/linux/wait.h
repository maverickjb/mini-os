#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#include <linux/spinlock.h>

struct task_struct;

struct wait_queue_entry {
    struct task_struct *task;
    struct wait_queue_entry *next;
};

struct wait_queue_head {
    spinlock_t lock;
    struct wait_queue_entry *head;
};

#define __WAITQUEUE_INITIALIZER(name, tsk) \
    { .task = (tsk), .next = NULL }

#define DECLARE_WAITQUEUE(name, tsk) \
    struct wait_queue_entry name = __WAITQUEUE_INITIALIZER(name, tsk)

void init_waitqueue_head(struct wait_queue_head *wq);
void add_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *entry);
void remove_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *entry);
void prepare_to_wait(struct wait_queue_head *wq, struct wait_queue_entry *entry);
void finish_wait(struct wait_queue_head *wq, struct wait_queue_entry *entry);
void wake_up(struct wait_queue_head *wq);
int wait_event(struct wait_queue_head *wq, int (*condition)(void));
long wait_event_interruptible(struct wait_queue_head *wq,
                              int (*condition)(void));

#endif /* _LINUX_WAIT_H */

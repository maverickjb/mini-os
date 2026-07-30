#ifndef _LINUX_SCHED_TASK_H
#define _LINUX_SCHED_TASK_H

#include <linux/sched.h>

#define current get_current()

struct task_struct *get_current(void);
void set_current(struct task_struct *task);

extern struct task_struct *kernel_thread(void (*fn)(void *), void *arg);
void kernel_init(void *arg);

#endif /* _LINUX_SCHED_TASK_H */
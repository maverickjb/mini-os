#ifndef FORK_H
#define FORK_H

#include "linux/sched.h"

#define current get_current()

struct task_struct *get_current(void);
void set_current(struct task_struct *task);

struct task_struct *kernel_thread(void (*fn)(void *), void *arg);
void wake_up_process(struct task_struct *task);
void kernel_init(void *arg);

#endif

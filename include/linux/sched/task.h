#ifndef _LINUX_SCHED_TASK_H
#define _LINUX_SCHED_TASK_H

#include <linux/sched.h>

#define current get_current()

struct task_struct *get_current(void);
void set_current(struct task_struct *task);

extern struct task_struct *kernel_thread(void (*fn)(void *), void *arg);
void wake_up_process(struct task_struct *task);
void kernel_init(void *arg);

void save_user_regs(struct task_struct *task, struct pt_regs *regs);
void restore_user_regs(struct task_struct *task, struct pt_regs *regs);

#endif /* _LINUX_SCHED_TASK_H */

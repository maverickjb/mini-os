#ifndef _LINUX_SCHED_TASK_H
#define _LINUX_SCHED_TASK_H

#include <linux/sched.h>

#define current get_current()

struct task_struct *get_current(void);
void set_current(struct task_struct *task);

extern struct task_struct *kernel_thread(void (*)(void *), void *arg);
void wake_up_process(struct task_struct *task);
void kernel_init(void *arg);

void copy_pt_regs(struct pt_regs *dst, const struct pt_regs *src);
void switch_to(struct task_struct *prev, struct task_struct *next);

void task_user_ctx_init(struct task_struct *task);
void ret_from_fork(void) __attribute__((noreturn));
void finish_eret(struct pt_regs *regs) __attribute__((noreturn));
void do_exit(long code) __attribute__((noreturn));

#endif /* _LINUX_SCHED_TASK_H */

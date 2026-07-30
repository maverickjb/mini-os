#ifndef __LINUX_SCHED_H
#define __LINUX_SCHED_H

#include <linux/mm_types.h>
#include <linux/fs.h>
#include <asm/ptrace.h>

#define INIT_STACK_SIZE   4096
#define NR_OPEN           8
#define SCHED_TIME_SLICE  10

struct task_struct {
    unsigned long pid;
    volatile unsigned long state;
    unsigned long *saved_sp;
    void (*thread_fn)(void *);
    void *thread_arg;
    unsigned long *stack;
    struct mm_struct *mm;
    struct file *files[NR_OPEN];
    struct task_struct *next;
    int time_slice;
    int is_user;
    unsigned long user_sp;
    struct pt_regs user_regs;
};

#define TASK_RUNNING    0
#define TASK_IDLE       1
#define TASK_SLEEPING   2

extern struct task_struct idle_tasks[];

void sched_init(void);
void sched_init_idle(unsigned int cpu);
void rest_init(void);
void cpu_idle(void);
void schedule(struct pt_regs *regs);
void enqueue_task(struct task_struct *task);
struct task_struct *pick_next_task(struct task_struct *prev);

#endif

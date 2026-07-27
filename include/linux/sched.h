#ifndef __LINUX_SCHED_H
#define __LINUX_SCHED_H

#include "linux/mm_types.h"

#define INIT_STACK_SIZE 4096

struct task_struct {
    unsigned long pid;
    volatile unsigned long state;
    unsigned long *saved_sp;
    void (*thread_fn)(void *);
    void *thread_arg;
    unsigned long *stack;
    struct mm_struct *mm;
    struct task_struct *next;
};

#define TASK_RUNNING    0
#define TASK_IDLE       1
#define TASK_SLEEPING   2

void sched_init(void);
void sched_init_idle(unsigned int cpu);
void rest_init(void);
void cpu_idle(void);
void schedule(void);

#endif
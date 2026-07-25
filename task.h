#ifndef TASK_H
#define TASK_H

#include "smp.h"

#define INIT_STACK_SIZE 4096

struct task_struct {
    unsigned long pid;
    volatile unsigned long state;
    unsigned long *saved_sp;
    void (*thread_fn)(void *);
    void *thread_arg;
    unsigned long *stack;
    struct task_struct *next;
};

#define TASK_RUNNING    0
#define TASK_IDLE       1
#define TASK_SLEEPING   2

extern struct task_struct init_task;

struct task_struct *get_current(void);
void set_current(struct task_struct *task);

struct task_struct *kernel_thread(void (*fn)(void *), void *arg);
void wake_up_process(struct task_struct *task);
void kernel_init(void *arg);

#endif

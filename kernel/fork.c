/*
 * Task management — init (PID 1) and kernel threads.
 */

#include <linux/sched.h>
 
#include "fork.h"
#include "page_alloc.h"

extern void task_trampoline(void);

static unsigned long next_pid = 1;

__attribute__((noinline))
static void task_frame_init(struct task_struct *task, void (*fn)(void *), void *arg)
{
    unsigned long *sp = task->stack + (INIT_STACK_SIZE / sizeof(unsigned long)) - 12;
    unsigned int i;

    for (i = 0; i < 12; i++)
        sp[i] = 0;

    sp[0] = (unsigned long)arg;
    sp[1] = (unsigned long)fn;
    sp[11] = (unsigned long)task_trampoline;
    task->saved_sp = sp;
}

struct task_struct *kernel_thread(void (*fn)(void *), void *arg)
{
    struct task_struct *tsk;
    unsigned long *stack;

    tsk = alloc_pages(0);
    if (!tsk)
        return 0;

    stack = alloc_pages(0);
    if (!stack)
        return 0;

    tsk->pid = next_pid++;
    tsk->state = TASK_SLEEPING;
    tsk->thread_fn = fn;
    tsk->thread_arg = arg;
    tsk->next = 0;
    tsk->stack = stack;
    tsk->saved_sp = 0;
    tsk->mm = 0;

    for (unsigned int i = 0; i < NR_OPEN; i++)
        tsk->files[i] = 0;

    task_frame_init(tsk, fn, arg);
    return tsk;
}

void wake_up_process(struct task_struct *task)
{
    task->state = TASK_RUNNING;
}


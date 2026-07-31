/*
 * Task management — kernel threads and user fork().
 */

#include <linux/sched/task.h>
#include <linux/errno.h>
#include <linux/stddef.h>

#include "page_alloc.h"
#include "mmap.h"

extern void task_trampoline(void);

static unsigned long next_pid = 1;

static void __attribute__((noinline)) copy_pt_regs(struct pt_regs *dst, const struct pt_regs *src)
{
    unsigned long *d = (unsigned long *)dst;
    const unsigned long *s = (const unsigned long *)src;
    unsigned int i;
    unsigned int n = sizeof(*dst) / sizeof(unsigned long);

    for (i = 0; i < n; i++)
        d[i] = s[i];
}

static void task_zero(struct task_struct *tsk)
{
    unsigned int i;

    tsk->pid = 0;
    tsk->state = TASK_SLEEPING;
    tsk->saved_sp = NULL;
    tsk->thread_fn = NULL;
    tsk->thread_arg = NULL;
    tsk->stack = NULL;
    tsk->mm = NULL;
    tsk->next = NULL;
    tsk->time_slice = 0;
    tsk->is_user = 0;
    tsk->user_sp = 0;

    for (i = 0; i < NR_OPEN; i++)
        tsk->files[i] = NULL;
}

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

void save_user_regs(struct task_struct *task, struct pt_regs *regs)
{
    copy_pt_regs(&task->user_regs, regs);
    __asm__ volatile("mrs %0, sp_el0" : "=r"(task->user_sp));
}

void restore_user_regs(struct task_struct *task, struct pt_regs *regs)
{
    copy_pt_regs(regs, &task->user_regs);
    __asm__ volatile("msr sp_el0, %0" : : "r"(task->user_sp));
    if (task->mm)
        mm_install(task->mm);
}

static void copy_task_files(struct task_struct *child, struct task_struct *parent)
{
    unsigned int i;

    for (i = 0; i < NR_OPEN; i++)
        child->files[i] = parent->files[i];
}

struct task_struct *kernel_thread(void (*fn)(void *), void *arg)
{
    struct task_struct *tsk;
    unsigned long *stack;

    tsk = alloc_pages(0);
    if (!tsk)
        return NULL;

    stack = alloc_pages(0);
    if (!stack)
        return NULL;

    task_zero(tsk);
    tsk->pid = next_pid++;
    tsk->state = TASK_SLEEPING;
    tsk->thread_fn = fn;
    tsk->thread_arg = arg;
    tsk->stack = stack;

    task_frame_init(tsk, fn, arg);
    return tsk;
}

void wake_up_process(struct task_struct *task)
{
    task->state = TASK_RUNNING;
    task->time_slice = SCHED_TIME_SLICE;
}

long ksys_fork(struct pt_regs *regs)
{
    struct task_struct *parent = current;
    struct task_struct *child;

    if (!parent || !parent->is_user)
        return -EINVAL;

    child = alloc_pages(0);
    if (!child)
        return -ENOMEM;

    task_zero(child);
    save_user_regs(parent, regs);

    child->pid = next_pid++;
    child->state = TASK_RUNNING;
    child->time_slice = SCHED_TIME_SLICE;
    child->is_user = 1;
    child->mm = parent->mm;
    child->user_sp = parent->user_sp;
    copy_pt_regs(&child->user_regs, &parent->user_regs);
    child->user_regs.x0 = 0;
    copy_task_files(child, parent);

    enqueue_task(child);
    return (long)child->pid;
}

void ksys_sched_yield(struct pt_regs *regs)
{
    if (!current || !current->is_user)
        return;

    schedule(regs);
}

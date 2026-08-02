/*
 * Process exit — terminate a user task and switch to the next runnable task.
 */

#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/serial.h>
#include <linux/stddef.h>

#include "mmap.h"

static void exit_files(struct task_struct *task)
{
    unsigned int i;

    for (i = 0; i < NR_OPEN; i++)
        task->files[i] = NULL;
}

static void exit_mm(struct task_struct *task)
{
    struct mm_struct *mm = task->mm;

    task->mm = NULL;
    mm_put(mm);
}

void ksys_exit(struct pt_regs *regs, long status)
{
    struct task_struct *task = current;

    if (!task || !task->is_user)
        return;

    task->exit_code = status;
    exit_files(task);
    exit_mm(task);
    task->state = TASK_ZOMBIE;

    schedule(regs);

    uart_puts("ksys_exit: schedule returned (bug)\n");
    for (;;)
        __asm__ volatile("wfi");
}

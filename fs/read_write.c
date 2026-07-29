/*
 * read(2)/write(2) syscalls — vfs entry points.
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include "fork.h"

static void enable_el1_user_access(void)
{
    unsigned long sctlr;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1UL << 22); /* UAO */
    __asm__ volatile("msr sctlr_el1, %0" : : "r"(sctlr));
    __asm__ volatile("isb");
}

long vfs_write(struct file *file, const char *buf, unsigned long count)
{
    if (!file || !file->f_op || !file->f_op->write)
        return -22;

    enable_el1_user_access();
    return file->f_op->write(file, buf, count, &file->f_pos);
}

long ksys_write(unsigned long fd, const char *buf, unsigned long count)
{
    struct task_struct *task = get_current();
    struct file *file;

    if (!task || fd >= NR_OPEN)
        return -9;

    file = task->files[fd];
    if (!file)
        return -9;

    return vfs_write(file, buf, count);
}

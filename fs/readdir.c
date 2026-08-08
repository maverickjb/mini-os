/*
 * getdents64(2) — read directory entries into a linux_dirent64 buffer.
 */

#include <linux/fs.h>
#include <linux/dirent.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/stddef.h>

long ksys_getdents64(unsigned long fd, void *dirp, unsigned long count)
{
    struct task_struct *task = current;
    struct file *file;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    if (!file->f_op || !file->f_op->readdir)
        return -ENOTDIR;

    return file->f_op->readdir(file, dirp, count);
}

/*
 * read(2)/write(2) syscalls — vfs entry points.
 */

#include <linux/fs.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

long vfs_write(struct file *file, const char *buf, unsigned long count)
{
    char kbuf[128];
    unsigned long n = count;

    if (!file || !file->f_op || !file->f_op->write)
        return -EINVAL;

    if (n > sizeof(kbuf))
        n = sizeof(kbuf);

    if (copy_from_user(kbuf, buf, n))
        return -EFAULT;

    return file->f_op->write(file, kbuf, n, &file->f_pos);
}

long vfs_read(struct file *file, char *buf, unsigned long count)
{
    char kbuf[128];
    unsigned long n = count;
    long ret;

    if (!file || !file->f_op || !file->f_op->read)
        return -EINVAL;

    if (n > sizeof(kbuf))
        n = sizeof(kbuf);

    ret = file->f_op->read(file, kbuf, n, &file->f_pos);
    if (ret <= 0)
        return ret;

    if (copy_to_user(buf, kbuf, (unsigned long)ret))
        return -EFAULT;

    return ret;
}

long ksys_write(unsigned long fd, const char *buf, unsigned long count)
{
    struct task_struct *task = get_current();
    struct file *file;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    return vfs_write(file, buf, count);
}

long ksys_read(unsigned long fd, char *buf, unsigned long count)
{
    struct task_struct *task = get_current();
    struct file *file;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    return vfs_read(file, buf, count);
}

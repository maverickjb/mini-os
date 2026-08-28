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

#define UIO_MAXIOV 1024

struct iovec {
    void *iov_base;
    unsigned long iov_len;
};

long ksys_writev(unsigned long fd, const void *uiov, unsigned long iovcnt)
{
    struct task_struct *task = get_current();
    struct file *file;
    const struct iovec *iov = (const struct iovec *)uiov;
    unsigned long i;
    long total = 0;

    if (!task || fd >= NR_OPEN)
        return -EBADF;
    if (!iov || iovcnt == 0)
        return 0;
    if (iovcnt > UIO_MAXIOV)
        return -EINVAL;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    for (i = 0; i < iovcnt; i++) {
        struct iovec vec;
        long ret;

        if (copy_from_user(&vec, &iov[i], sizeof(vec)))
            return total ? total : -EFAULT;
        if (vec.iov_len == 0)
            continue;
        if (!vec.iov_base)
            return total ? total : -EFAULT;

        ret = vfs_write(file, (const char *)vec.iov_base, vec.iov_len);
        if (ret < 0)
            return total ? total : ret;

        total += ret;
        if ((unsigned long)ret < vec.iov_len)
            break;
    }

    return total;
}

long ksys_sendfile(unsigned long out_fd, unsigned long in_fd, long *offset,
                   unsigned long count)
{
    struct task_struct *task = get_current();
    struct file *in;
    struct file *out;
    long pos;
    long *ppos;
    unsigned long done = 0;
    char kbuf[128];

    if (!task || out_fd >= NR_OPEN || in_fd >= NR_OPEN)
        return -EBADF;

    out = task->files[out_fd];
    in = task->files[in_fd];
    if (!out || !in)
        return -EBADF;
    if (!in->f_op || !in->f_op->read)
        return -EINVAL;
    if (!out->f_op || !out->f_op->write)
        return -EINVAL;

    if (offset) {
        if (copy_from_user(&pos, offset, sizeof(pos)))
            return -EFAULT;
        ppos = &pos;
    } else {
        ppos = &in->f_pos;
    }

    while (done < count) {
        unsigned long chunk = count - done;
        long nr;
        long nw;

        if (chunk > sizeof(kbuf))
            chunk = sizeof(kbuf);

        nr = in->f_op->read(in, kbuf, chunk, ppos);
        if (nr < 0)
            return done ? (long)done : nr;
        if (nr == 0)
            break;

        nw = out->f_op->write(out, kbuf, (unsigned long)nr, &out->f_pos);
        if (nw < 0)
            return done ? (long)done : nw;
        if (nw == 0)
            break;

        done += (unsigned long)nw;
        if (nw < nr) {
            /* Partial write: rewind unused input bytes. */
            *ppos -= (nr - nw);
            break;
        }
    }

    if (offset && copy_to_user(offset, &pos, sizeof(pos)))
        return -EFAULT;

    return (long)done;
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

long ksys_ioctl(unsigned long fd, unsigned int cmd, unsigned long arg)
{
    struct task_struct *task = get_current();
    struct file *file;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    if (!file->f_op || !file->f_op->ioctl)
        return -ENOTTY;

    return file->f_op->ioctl(file, cmd, arg);
}

loff_t generic_file_llseek(struct file *file, loff_t offset, int whence)
{
    loff_t pos;

    switch (whence) {
    case SEEK_SET:
        pos = offset;
        break;
    case SEEK_CUR:
        pos = (loff_t)file->f_pos + offset;
        break;
    case SEEK_END:
        pos = (file->inode ? (loff_t)file->inode->size : 0) + offset;
        break;
    default:
        return -EINVAL;
    }

    if (pos < 0)
        return -EINVAL;

    file->f_pos = (long)pos;
    return pos;
}

loff_t vfs_llseek(struct file *file, loff_t offset, int whence)
{
    if (!(file->f_mode & FMODE_LSEEK))
        return -ESPIPE;

    return file->f_op->llseek(file, offset, whence);
}

long ksys_lseek(unsigned int fd, off_t offset, unsigned int whence)
{
    struct task_struct *task = get_current();
    struct file *file;
    loff_t res;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    res = vfs_llseek(file, (loff_t)offset, (int)whence);
    return (long)res;
}

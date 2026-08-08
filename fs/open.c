/*
 * open(2) / close(2) / dup — fd table and file refcounting.
 */

#include <linux/fs.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/stddef.h>
#include <linux/serial.h>

#include <linux/namei.h>
#include <linux/ramfs.h>

#define PATH_MAX 256

void get_file(struct file *file)
{
    if (file)
        file->refcount++;
}

void fput(struct file *file)
{
    if (!file)
        return;

    file->refcount--;
    if (file->refcount > 0)
        return;

    if (file->f_op && file->f_op->release)
        file->f_op->release(file);

    /* uart_file is static and must never be freed. */
    if (file != &uart_file)
        free_pages(file, 0);
}

struct file *alloc_file(void)
{
    struct file *file;

    file = alloc_pages(0);
    if (!file)
        return NULL;

    file->refcount = 1;
    file->inode = NULL;
    file->f_op = NULL;
    file->private_data = NULL;
    file->f_pos = 0;
    file->f_flags = 0;
    return file;
}

int install_fd(struct task_struct *task, struct file *file)
{
    int fd;

    for (fd = 0; fd < NR_OPEN; fd++) {
        if (!task->files[fd]) {
            task->files[fd] = file;
            return fd;
        }
    }

    return -EMFILE;
}

static long close_fd(struct task_struct *task, unsigned long fd)
{
    struct file *file;

    if (fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    task->files[fd] = NULL;
    fput(file);
    return 0;
}

long ksys_open(const char *filename, int flags, unsigned long mode)
{
    char path[PATH_MAX];
    struct inode *inode;
    struct file *file;
    struct task_struct *task = current;
    int err;
    int fd;

    (void)mode;

    if (!task || !task->is_user)
        return -EINVAL;

    err = strncpy_from_user(path, filename, PATH_MAX);
    if (err < 0)
        return err;
    if (err >= PATH_MAX)
        return -ENAMETOOLONG;

    inode = vfs_lookup(path);
    if (!inode) {
        if (!(flags & O_CREAT))
            return -ENOENT;

        err = ramfs_create(path);
        if (err)
            return err;

        inode = vfs_lookup(path);
        if (!inode)
            return -ENOENT;
    }

    if (inode_is_dir(inode)) {
        if ((flags & O_ACCMODE) != O_RDONLY)
            return -EISDIR;
    } else if (flags & O_DIRECTORY) {
        return -ENOTDIR;
    } else if (!inode_is_reg(inode)) {
        return -ENOENT;
    }

    if (!inode_is_dir(inode) && (flags & O_TRUNC) &&
        (flags & O_ACCMODE) != O_RDONLY) {
        err = ramfs_truncate(inode, 0);
        if (err)
            return err;
    }

    file = alloc_file();
    if (!file)
        return -ENOMEM;

    file->inode = inode;
    file->f_op = inode->ops;
    file->private_data = inode->private_data;
    file->f_pos = 0;
    file->f_flags = flags;

    fd = install_fd(task, file);
    if (fd < 0) {
        fput(file);
        return fd;
    }

    return fd;
}

long ksys_openat(int dfd, const char *filename, int flags, unsigned long mode)
{
    /*
     * Absolute paths only for now; dfd is ignored when path starts with '/'.
     */
    (void)dfd;
    return ksys_open(filename, flags, mode);
}

long ksys_close(unsigned long fd)
{
    struct task_struct *task = current;

    if (!task)
        return -EBADF;

    return close_fd(task, fd);
}

long ksys_dup(unsigned long oldfd)
{
    struct task_struct *task = current;
    struct file *file;
    int fd;

    if (!task || oldfd >= NR_OPEN)
        return -EBADF;

    file = task->files[oldfd];
    if (!file)
        return -EBADF;

    get_file(file);
    fd = install_fd(task, file);
    if (fd < 0) {
        fput(file);
        return fd;
    }

    return fd;
}

long ksys_dup2(unsigned long oldfd, unsigned long newfd)
{
    return ksys_dup3(oldfd, newfd, 0);
}

long ksys_dup3(unsigned long oldfd, unsigned long newfd, int flags)
{
    struct task_struct *task = current;
    struct file *file;
    struct file *old_new;

    (void)flags;

    if (!task || oldfd >= NR_OPEN || newfd >= NR_OPEN)
        return -EBADF;

    file = task->files[oldfd];
    if (!file)
        return -EBADF;

    if (oldfd == newfd)
        return (long)newfd;

    get_file(file);
    old_new = task->files[newfd];
    task->files[newfd] = file;
    if (old_new)
        fput(old_new);

    return (long)newfd;
}

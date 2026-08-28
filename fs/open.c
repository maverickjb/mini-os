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
#include <linux/proc_fs.h>
#include <linux/devnull.h>
#include <linux/devtty.h>

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
    file->f_mode = 0;
    return file;
}

int install_fd(struct task_struct *task, struct file *file)
{
    int fd;

    for (fd = 0; fd < NR_OPEN; fd++) {
        if (!task->files[fd]) {
            task->files[fd] = file;
            task->close_on_exec &= ~(1UL << fd);
            return fd;
        }
    }

    return -EMFILE;
}

static int install_fd_min(struct task_struct *task, struct file *file,
                          unsigned int minfd)
{
    int fd;

    if (minfd >= NR_OPEN)
        return -EINVAL;

    for (fd = (int)minfd; fd < NR_OPEN; fd++) {
        if (!task->files[fd]) {
            task->files[fd] = file;
            task->close_on_exec &= ~(1UL << fd);
            return fd;
        }
    }

    return -EMFILE;
}

void close_on_exec_fds(struct task_struct *task)
{
    unsigned int i;

    if (!task)
        return;

    for (i = 0; i < NR_OPEN; i++) {
        if (!(task->close_on_exec & (1UL << i)))
            continue;

        if (task->files[i]) {
            fput(task->files[i]);
            task->files[i] = NULL;
        }
        task->close_on_exec &= ~(1UL << i);
    }
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

    err = getname_from_user(path, filename);
    if (err)
        return err;

    if (devnull_is_path(path)) {
        if (flags & O_DIRECTORY)
            return -ENOTDIR;

        file = devnull_open(flags);
        if (!file)
            return -ENOMEM;

        fd = install_fd(task, file);
        if (fd < 0) {
            fput(file);
            return fd;
        }

        return fd;
    }

    if (devtty_is_path(path)) {
        if (flags & O_DIRECTORY)
            return -ENOTDIR;

        file = devtty_open(flags);
        if (!file)
            return -ENXIO;

        fd = install_fd(task, file);
        if (fd < 0) {
            fput(file);
            return fd;
        }

        return fd;
    }

    /* No creating files under /proc. */
    if (path[0] == '/' && path[1] == 'p' && path[2] == 'r' &&
        path[3] == 'o' && path[4] == 'c' && path[5] == '/' &&
        (flags & O_CREAT))
        return -EACCES;

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
        if ((flags & O_ACCMODE) != O_RDONLY) {
            proc_iput(inode);
            return -EISDIR;
        }
    } else if (flags & O_DIRECTORY) {
        proc_iput(inode);
        return -ENOTDIR;
    } else if (!inode_is_reg(inode)) {
        proc_iput(inode);
        return -ENOENT;
    }

    if (!inode_is_dir(inode) && (flags & O_TRUNC) &&
        (flags & O_ACCMODE) != O_RDONLY) {
        if (proc_is_inode(inode)) {
            proc_iput(inode);
            return -EACCES;
        }
        err = ramfs_truncate(inode, 0);
        if (err) {
            proc_iput(inode);
            return err;
        }
    }

    file = alloc_file();
    if (!file) {
        proc_iput(inode);
        return -ENOMEM;
    }

    file->inode = inode;
    file->f_op = (struct file_ops *)inode->i_fop;
    file->private_data = inode->private_data;
    file->f_pos = 0;
    file->f_flags = flags;
    file->f_mode = 0;
    if (inode_is_reg(inode) && file->f_op && file->f_op->llseek)
        file->f_mode |= FMODE_LSEEK;

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

    if (!task || oldfd >= NR_OPEN || newfd >= NR_OPEN)
        return -EBADF;

    if (flags & ~O_CLOEXEC)
        return -EINVAL;

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

    if (flags & O_CLOEXEC)
        task->close_on_exec |= (1UL << newfd);
    else
        task->close_on_exec &= ~(1UL << newfd);

    return (long)newfd;
}

long ksys_fcntl(unsigned long fd, unsigned int cmd, unsigned long arg)
{
    struct task_struct *task = current;
    struct file *file;
    int newfd;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC: {
        unsigned int minfd = (unsigned int)arg;

        if ((int)minfd < 0 || minfd >= NR_OPEN)
            return -EINVAL;

        get_file(file);
        newfd = install_fd_min(task, file, minfd);
        if (newfd < 0) {
            fput(file);
            return newfd;
        }

        if (cmd == F_DUPFD_CLOEXEC)
            task->close_on_exec |= (1UL << newfd);
        else
            task->close_on_exec &= ~(1UL << newfd);

        return newfd;
    }
    case F_GETFD:
        return (task->close_on_exec & (1UL << fd)) ? FD_CLOEXEC : 0;
    case F_SETFD:
        if (arg & ~FD_CLOEXEC)
            return -EINVAL;
        if (arg & FD_CLOEXEC)
            task->close_on_exec |= (1UL << fd);
        else
            task->close_on_exec &= ~(1UL << fd);
        return 0;
    case F_GETFL:
        return file->f_flags;
    case F_SETFL:
        file->f_flags = (file->f_flags & ~(O_APPEND | O_NONBLOCK | O_ASYNC)) |
                        (int)(arg & (O_APPEND | O_NONBLOCK | O_ASYNC));
        return 0;
    default:
        return -EINVAL;
    }
}

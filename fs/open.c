/*
 * open(2) — look up a path, attach inode, install an fd.
 */

#include <linux/fs.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/stddef.h>

#include "namei.h"
#include "ramfs.h"

#define PATH_MAX 256

static int copy_path_from_user(char *dst, const char *src, unsigned long max)
{
    unsigned long i;

    if (!src || max == 0)
        return -EINVAL;

    for (i = 0; i < max; i++) {
        if (copy_from_user(dst + i, src + i, 1))
            return -EFAULT;
        if (dst[i] == '\0')
            return 0;
    }

    return -ENAMETOOLONG;
}

static struct file *alloc_file(void)
{
    struct file *file;

    file = alloc_pages(0);
    if (!file)
        return NULL;

    file->inode = NULL;
    file->f_op = NULL;
    file->private_data = NULL;
    file->f_pos = 0;
    file->f_flags = 0;
    return file;
}

static int install_fd(struct task_struct *task, struct file *file)
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

    err = copy_path_from_user(path, filename, PATH_MAX);
    if (err)
        return err;

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

    if (inode_is_dir(inode))
        return -EISDIR;

    if (!inode_is_reg(inode))
        return -ENOENT;

    if ((flags & O_TRUNC) && (flags & O_ACCMODE) != O_RDONLY) {
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
        free_pages(file, 0);
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

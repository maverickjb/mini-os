/*
 * fstat(2) / newfstatat(2) — fill a Linux aarch64 struct stat.
 */

#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/serial.h>

#include <linux/namei.h>

#define PATH_MAX 256

static void cp_inode_stat(struct inode *inode, struct stat *st)
{
    memset(st, 0, sizeof(*st));

    if (!inode) {
        st->st_mode = S_IFCHR | 0666;
        st->st_nlink = 1;
        st->st_blksize = 1024;
        return;
    }

    st->st_ino = inode->ino;
    st->st_mode = (unsigned int)inode->type;
    if (inode_is_dir(inode))
        st->st_mode |= 0755;
    else if (inode_is_reg(inode))
        st->st_mode |= 0644;
    else
        st->st_mode |= 0644;

    st->st_nlink = 1;
    st->st_size = (long)inode->size;
    st->st_blksize = 1024;
    st->st_blocks = (st->st_size + 511) / 512;
}

static void cp_file_stat(struct file *file, struct stat *st)
{
    if (!file) {
        memset(st, 0, sizeof(*st));
        return;
    }

    if (file->inode) {
        cp_inode_stat(file->inode, st);
        return;
    }

    /* Anonymous pipe / tty without a backing inode. */
    memset(st, 0, sizeof(*st));
    if (file == &uart_file) {
        st->st_mode = S_IFCHR | 0666;
    } else {
        st->st_mode = S_IFIFO | 0600;
    }
    st->st_nlink = 1;
    st->st_blksize = 1024;
}

static long do_stat_inode(struct inode *inode, struct stat *user_stat)
{
    struct stat st;

    if (!user_stat)
        return -EFAULT;

    cp_inode_stat(inode, &st);
    if (copy_to_user(user_stat, &st, sizeof(st)))
        return -EFAULT;

    return 0;
}

static long do_stat_file(struct file *file, struct stat *user_stat)
{
    struct stat st;

    if (!user_stat)
        return -EFAULT;

    cp_file_stat(file, &st);
    if (copy_to_user(user_stat, &st, sizeof(st)))
        return -EFAULT;

    return 0;
}

long ksys_fstat(unsigned long fd, struct stat *statbuf)
{
    struct task_struct *task = current;
    struct file *file;

    if (!task || fd >= NR_OPEN)
        return -EBADF;

    file = task->files[fd];
    if (!file)
        return -EBADF;

    return do_stat_file(file, statbuf);
}

long ksys_newfstatat(int dfd, const char *filename, struct stat *statbuf,
                     int flag)
{
    char path[PATH_MAX];
    struct inode *inode;
    struct task_struct *task = current;
    int err;

    (void)flag;

    if (!task || !task->is_user)
        return -EINVAL;

    if (!filename)
        return -EFAULT;

    err = strncpy_from_user(path, filename, PATH_MAX);
    if (err < 0)
        return err;
    if (err >= PATH_MAX)
        return -ENAMETOOLONG;

    /* AT_EMPTY_PATH: empty pathname refers to dfd. */
    if (path[0] == '\0') {
        struct file *file;

        if (!(flag & AT_EMPTY_PATH))
            return -ENOENT;

        if (dfd < 0 || (unsigned long)dfd >= NR_OPEN)
            return -EBADF;

        file = task->files[dfd];
        if (!file)
            return -EBADF;

        return do_stat_file(file, statbuf);
    }

    /*
     * Absolute paths only for now; dfd is ignored when path starts with '/'.
     */
    (void)dfd;

    inode = vfs_lookup(path);
    if (!inode)
        return -ENOENT;

    return do_stat_inode(inode, statbuf);
}

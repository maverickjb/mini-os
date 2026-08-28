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
#include <linux/proc_fs.h>
#include <linux/devnull.h>

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

    st->st_nlink = inode->nlink ? inode->nlink : 1;
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

    /* Anonymous pipe / tty / devnull without a backing inode. */
    memset(st, 0, sizeof(*st));
    if (file == &uart_file) {
        st->st_mode = S_IFCHR | 0666;
    } else if (devnull_file_is(file)) {
        devnull_fill_stat(st);
        return;
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

    /* AT_EMPTY_PATH: empty pathname refers to dfd. */
    if (flag & AT_EMPTY_PATH) {
        char empty[1];
        long n;

        n = strncpy_from_user(empty, filename, 1);
        if (n < 0)
            return n;
        if (n == 0) {
            struct file *file;

            if (dfd < 0 || (unsigned long)dfd >= NR_OPEN)
                return -EBADF;

            file = task->files[dfd];
            if (!file)
                return -EBADF;

            return do_stat_file(file, statbuf);
        }
    }

    (void)dfd;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    if (devnull_is_path(path)) {
        struct stat st;

        devnull_fill_stat(&st);
        if (copy_to_user(statbuf, &st, sizeof(st)))
            return -EFAULT;
        return 0;
    }

    inode = vfs_lookup(path);
    if (!inode)
        return -ENOENT;

    err = (int)do_stat_inode(inode, statbuf);
    proc_iput(inode);
    return err;
}

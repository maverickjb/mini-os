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
#include <linux/ramfs.h>
#include <linux/proc_fs.h>
#include <linux/devnull.h>
#include <linux/devtty.h>
#include <linux/devconsole.h>

static umode_t inode_mode(struct inode *inode)
{
    umode_t mode;

    if (!inode)
        return (umode_t)(S_IFCHR | 0666);

    mode = (umode_t)inode->type;
    if (inode_is_dir(inode) || inode_is_reg(inode))
        mode |= 0755;
    else if (inode_is_lnk(inode))
        mode |= 0777;
    else if (inode_is_chr(inode)) {
        if (inode->i_rdev == ((5U << 8) | 1U))
            mode |= 0600;
        else
            mode |= 0666;
    } else
        mode |= 0644;

    return mode;
}

static int inode_access(struct inode *inode, int mode)
{
    umode_t m = inode_mode(inode);

    if ((mode & R_OK) && !(m & 0444))
        return -EACCES;
    if ((mode & W_OK) && !(m & 0222))
        return -EACCES;
    if ((mode & X_OK) && !(m & 0111))
        return -EACCES;
    return 0;
}

static void cp_inode_stat(struct inode *inode, struct stat *st)
{
    memset(st, 0, sizeof(*st));

    if (!inode) {
        st->st_mode = inode_mode(NULL);
        st->st_nlink = 1;
        st->st_blksize = 1024;
        return;
    }

    st->st_ino = inode->ino;
    st->st_mode = inode_mode(inode);
    if (inode_is_chr(inode))
        st->st_rdev = inode->i_rdev;
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
    if (file == &uart_file || devconsole_file_is(file)) {
        devconsole_fill_stat(st);
        return;
    } else if (devtty_file_is(file)) {
        devtty_fill_stat(st);
        return;
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

    inode = vfs_lookup(path);
    if (!inode)
        return -ENOENT;

    err = (int)do_stat_inode(inode, statbuf);
    proc_iput(inode);
    return err;
}

long ksys_faccessat2(int dfd, const char *filename, int mode, int flags)
{
    char path[PATH_MAX];
    struct inode *inode;
    struct task_struct *task = current;
    int err;

    if (!task || !task->is_user)
        return -EINVAL;

    if (mode & ~(R_OK | W_OK | X_OK))
        return -EINVAL;

    if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EACCESS | AT_EMPTY_PATH))
        return -EINVAL;

    if (!filename)
        return -EFAULT;

    if (flags & AT_EMPTY_PATH) {
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

            if (file->inode)
                return inode_access(file->inode, mode);

            /* Pipe / tty without a backing inode: rw, not exec. */
            if (mode & X_OK)
                return -EACCES;
            return 0;
        }
    }

    (void)dfd;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    if (flags & AT_SYMLINK_NOFOLLOW)
        inode = ramfs_lookup(path);
    else
        inode = vfs_lookup(path);

    if (!inode)
        return -ENOENT;

    err = inode_access(inode, mode);
    proc_iput(inode);
    return err;
}

long ksys_faccessat(int dfd, const char *filename, int mode)
{
    return ksys_faccessat2(dfd, filename, mode, 0);
}

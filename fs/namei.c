/*
 * Pathname lookup / mkdir — VFS entry points over ramfs.
 */

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/ramfs.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/stddef.h>

#define PATH_MAX 256

struct inode *vfs_lookup(const char *path)
{
    return ramfs_lookup(path);
}

int vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    if (!dir || !dentry)
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;
    if (!dir->i_op || !dir->i_op->mkdir)
        return -EPERM;

    return dir->i_op->mkdir(dir, dentry, mode);
}

long ksys_mkdirat(int dfd, const char *filename, umode_t mode)
{
    char path[PATH_MAX];
    long n;

    (void)dfd;
    (void)mode;

    if (!current || !current->is_user)
        return -EINVAL;

    if (!filename)
        return -EFAULT;

    n = strncpy_from_user(path, filename, PATH_MAX);
    if (n < 0)
        return n;
    if (n >= PATH_MAX)
        return -ENAMETOOLONG;
    if (path[0] != '/')
        return -EINVAL;

    return ramfs_mkdir(path);
}

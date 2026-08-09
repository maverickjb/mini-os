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

/*
 * Split "/a/b/c" into parent="/a/b" and name="c".
 * Root-only "/" has no creatable name.
 */
static int path_parent_name(const char *path, char *parent, char *name)
{
    const char *p;
    const char *slash;
    unsigned long i, len;

    if (!path || path[0] != '/')
        return -EINVAL;

    /* Trim trailing slashes (not the root slash itself). */
    len = 0;
    while (path[len])
        len++;
    while (len > 1 && path[len - 1] == '/')
        len--;

    if (len <= 1)
        return -ENOENT;

    slash = NULL;
    for (i = 0; i < len; i++) {
        if (path[i] == '/')
            slash = path + i;
    }

    if (!slash)
        return -EINVAL;

    /* Parent path: "/" or prefix through the last slash. */
    if (slash == path) {
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        unsigned long plen = (unsigned long)(slash - path);

        if (plen >= PATH_MAX)
            return -ENAMETOOLONG;
        for (i = 0; i < plen; i++)
            parent[i] = path[i];
        parent[plen] = '\0';
    }

    p = slash + 1;
    i = 0;
    while (p < path + len && i + 1 < DNAME_INLINE_LEN)
        name[i++] = *p++;
    name[i] = '\0';

    if (!name[0])
        return -ENOENT;

    return 0;
}

static int do_mkdir(const char *path, umode_t mode)
{
    char parent[PATH_MAX];
    struct dentry dentry;
    struct inode *dir;
    int err;

    err = path_parent_name(path, parent, dentry.name);
    if (err)
        return err;

    dir = vfs_lookup(parent);
    if (!dir)
        return -ENOENT;

    dentry.inode = NULL;
    dentry.parent = NULL;
    dentry.next = NULL;

    return vfs_mkdir(dir, &dentry, mode);
}

long ksys_mkdirat(int dfd, const char *filename, umode_t mode)
{
    char path[PATH_MAX];
    long n;

    (void)dfd;

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

    return do_mkdir(path, mode);
}

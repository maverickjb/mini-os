/*
 * Pathname lookup / mkdir / unlink / chdir — VFS entry points over ramfs.
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

int vfs_unlink(struct inode *dir, struct dentry *dentry)
{
    if (!dir || !dentry)
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;
    if (!dir->i_op || !dir->i_op->unlink)
        return -EPERM;
    if (dentry->inode && inode_is_dir(dentry->inode))
        return -EISDIR;

    return dir->i_op->unlink(dir, dentry);
}

int vfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    if (!dir || !dentry)
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;
    if (!dir->i_op || !dir->i_op->rmdir)
        return -EPERM;
    if (dentry->inode && !inode_is_dir(dentry->inode))
        return -ENOTDIR;

    return dir->i_op->rmdir(dir, dentry);
}

static unsigned long str_len(const char *s)
{
    unsigned long n = 0;

    while (s[n])
        n++;
    return n;
}

static void str_copy(char *dst, const char *src, unsigned long max)
{
    unsigned long i = 0;

    if (!max)
        return;
    while (src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/*
 * Resolve path against base (absolute cwd) into out.
 * Handles "." / ".." components; result is always absolute.
 */
static int path_resolve(const char *base, const char *path, char *out)
{
    char stack[PATH_MAX];
    unsigned long sp = 0;
    const char *p;
    unsigned long i;

    if (!path || !path[0] || !out)
        return -ENOENT;

    if (path[0] == '/') {
        stack[0] = '\0';
        sp = 0;
        p = path;
    } else {
        if (!base || base[0] != '/')
            return -EINVAL;
        str_copy(stack, base, PATH_MAX);
        sp = str_len(stack);
        p = path;
    }

    while (*p) {
        char comp[PATH_MAX];
        unsigned long clen = 0;

        while (*p == '/')
            p++;
        if (!*p)
            break;

        while (p[clen] && p[clen] != '/')
            clen++;
        if (clen >= PATH_MAX)
            return -ENAMETOOLONG;
        for (i = 0; i < clen; i++)
            comp[i] = p[i];
        comp[clen] = '\0';
        p += clen;

        if (comp[0] == '.' && comp[1] == '\0')
            continue;
        if (comp[0] == '.' && comp[1] == '.' && comp[2] == '\0') {
            /* pop last component */
            while (sp > 0 && stack[sp - 1] != '/')
                sp--;
            if (sp > 0)
                sp--; /* drop the slash */
            stack[sp] = '\0';
            continue;
        }

        if (sp + 1 + clen >= PATH_MAX)
            return -ENAMETOOLONG;
        if (sp == 0 || stack[sp - 1] != '/')
            stack[sp++] = '/';
        for (i = 0; i < clen; i++)
            stack[sp++] = comp[i];
        stack[sp] = '\0';
    }

    if (sp == 0) {
        out[0] = '/';
        out[1] = '\0';
    } else {
        str_copy(out, stack, PATH_MAX);
    }

    return 0;
}

long getname_from_user(char *buf, const char *filename)
{
    char userpath[PATH_MAX];
    const char *cwd;
    long n;
    int err;

    if (!current || !current->is_user)
        return -EINVAL;
    if (!filename || !buf)
        return -EFAULT;

    n = strncpy_from_user(userpath, filename, PATH_MAX);
    if (n < 0)
        return n;
    if (n >= PATH_MAX)
        return -ENAMETOOLONG;
    if (n == 0)
        return -ENOENT;

    cwd = current->cwd[0] ? current->cwd : "/";
    err = path_resolve(cwd, userpath, buf);
    if (err)
        return err;

    return 0;
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

static int lookup_parent_dentry(const char *path, struct inode **dir_out,
                                struct dentry *dentry)
{
    char parent[PATH_MAX];
    struct inode *dir;
    int err;

    err = path_parent_name(path, parent, dentry->name);
    if (err)
        return err;

    dir = vfs_lookup(parent);
    if (!dir)
        return -ENOENT;

    dentry->inode = vfs_lookup(path);
    if (!dentry->inode)
        return -ENOENT;

    dentry->parent = NULL;
    dentry->next = NULL;
    *dir_out = dir;
    return 0;
}

static int do_unlink(const char *path)
{
    struct dentry dentry;
    struct inode *dir;
    int err;

    err = lookup_parent_dentry(path, &dir, &dentry);
    if (err)
        return err;

    return vfs_unlink(dir, &dentry);
}

static int do_rmdir(const char *path)
{
    struct dentry dentry;
    struct inode *dir;
    int err;

    err = lookup_parent_dentry(path, &dir, &dentry);
    if (err)
        return err;

    return vfs_rmdir(dir, &dentry);
}

long ksys_mkdirat(int dfd, const char *filename, umode_t mode)
{
    char path[PATH_MAX];
    long err;

    (void)dfd;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    return do_mkdir(path, mode);
}

long ksys_rmdir(const char *pathname)
{
    char path[PATH_MAX];
    long err;

    err = getname_from_user(path, pathname);
    if (err)
        return err;

    return do_rmdir(path);
}

long ksys_unlinkat(int dfd, const char *filename, int flag)
{
    char path[PATH_MAX];
    long err;

    (void)dfd;

    if (flag & ~AT_REMOVEDIR)
        return -EINVAL;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    if (flag & AT_REMOVEDIR)
        return do_rmdir(path);

    return do_unlink(path);
}

long ksys_chdir(const char *filename)
{
    char path[PATH_MAX];
    struct inode *inode;
    long err;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    inode = vfs_lookup(path);
    if (!inode)
        return -ENOENT;
    if (!inode_is_dir(inode))
        return -ENOTDIR;

    str_copy(current->cwd, path, PATH_MAX);
    return 0;
}

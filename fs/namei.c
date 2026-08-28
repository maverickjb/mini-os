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
#include <linux/gfp.h>
#include <linux/proc_fs.h>

static int path_parent_name(const char *path, char *parent, char *name);
static int path_resolve(const char *base, const char *path, char *out);
static void str_copy(char *dst, const char *src, unsigned long max);

struct inode *vfs_lookup(const char *path)
{
    char curpath[PATH_MAX];
    char target[PATH_MAX];
    char resolved[PATH_MAX];
    char scratch[DNAME_INLINE_LEN];
    struct inode *inode;
    long len;
    int depth;
    int err;

    if (!path)
        return NULL;

    /* /proc and anything under it. */
    if (path[0] == '/' && path[1] == 'p' && path[2] == 'r' &&
        path[3] == 'o' && path[4] == 'c' &&
        (path[5] == '\0' || path[5] == '/')) {
        inode = proc_lookup(path);
        if (inode)
            return inode;
        /* Fall through for plain "/proc" if proc_init has not run. */
    }

    str_copy(curpath, path, PATH_MAX);

    for (depth = 0; depth < 8; depth++) {
        inode = ramfs_lookup(curpath);
        if (!inode)
            return NULL;

        if (!inode_is_lnk(inode))
            return inode;

        len = ramfs_readlink(inode, target, PATH_MAX - 1);
        if (len < 0)
            return NULL;
        target[len] = '\0';

        if (target[0] == '/') {
            str_copy(resolved, target, PATH_MAX);
        } else {
            char parent[PATH_MAX];

            err = path_parent_name(curpath, parent, scratch);
            if (err)
                return NULL;
            err = path_resolve(parent, target, resolved);
            if (err)
                return NULL;
        }

        str_copy(curpath, resolved, PATH_MAX);
    }

    return NULL;
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

int vfs_link(struct dentry *old_dentry, struct inode *dir,
             struct dentry *new_dentry)
{
    if (!old_dentry || !old_dentry->inode || !dir || !new_dentry)
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;
    if (!dir->i_op || !dir->i_op->link)
        return -EPERM;
    if (inode_is_dir(old_dentry->inode))
        return -EPERM;

    return dir->i_op->link(old_dentry, dir, new_dentry);
}

int vfs_symlink(const char *target, struct inode *dir, struct dentry *dentry)
{
    if (!target || !dir || !dentry)
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;
    if (!dir->i_op || !dir->i_op->symlink)
        return -EPERM;

    return dir->i_op->symlink(dir, dentry, target);
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

static struct dentry *pwd_dentry(void)
{
    if (current && current->cwd)
        return current->cwd;
    return d_root();
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
            while (sp > 0 && stack[sp - 1] != '/')
                sp--;
            if (sp > 0)
                sp--;
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
    char base[PATH_MAX];
    long n;
    long plen;
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

    plen = dentry_path(pwd_dentry(), base, PATH_MAX);
    if (plen < 0)
        return plen;

    err = path_resolve(base, userpath, buf);
    if (err)
        return err;

    return 0;
}

/*
 * Split "/a/b/c" into parent="/a/b" and name="c".
 */
static int path_parent_name(const char *path, char *parent, char *name)
{
    const char *p;
    const char *slash;
    unsigned long i, len;

    if (!path || path[0] != '/')
        return -EINVAL;

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
    char parent_path[PATH_MAX];
    char name[DNAME_INLINE_LEN];
    struct dentry *parent;
    struct dentry *d;
    int err;

    err = path_parent_name(path, parent_path, name);
    if (err)
        return err;

    parent = d_lookup_path(parent_path);
    if (!parent)
        return -ENOENT;

    if (d_lookup_path(path))
        return -EEXIST;

    d = d_alloc(parent, name);
    if (!d)
        return -ENOMEM;

    err = vfs_mkdir(parent->inode, d, mode);
    if (err) {
        /* d not linked yet */
        free_pages(d, 0);
        return err;
    }

    d_add(d);
    return 0;
}

static int do_unlink(const char *path)
{
    char parent_path[PATH_MAX];
    char name[DNAME_INLINE_LEN];
    struct dentry *parent;
    struct dentry tmp;
    int err;
    unsigned long i;

    err = path_parent_name(path, parent_path, name);
    if (err)
        return err;

    parent = d_lookup_path(parent_path);
    if (!parent)
        return -ENOENT;

    for (i = 0; i < sizeof(tmp.name); i++)
        tmp.name[i] = 0;
    str_copy(tmp.name, name, sizeof(tmp.name));
    tmp.inode = vfs_lookup(path);
    if (!tmp.inode)
        return -ENOENT;
    tmp.parent = parent;
    tmp.child = NULL;
    tmp.next = NULL;

    return vfs_unlink(parent->inode, &tmp);
}

static int do_rmdir(const char *path)
{
    struct dentry *d;
    struct dentry *parent;
    int err;

    d = d_lookup_path(path);
    if (!d || !d->parent)
        return -ENOENT;

    parent = d->parent;
    err = vfs_rmdir(parent->inode, d);
    if (err)
        return err;

    d_drop(d);
    return 0;
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

static int do_link(const char *oldpath, const char *newpath)
{
    char parent_path[PATH_MAX];
    char name[DNAME_INLINE_LEN];
    struct dentry *parent;
    struct dentry old_dentry;
    struct dentry new_dentry;
    struct inode *old_inode;
    int err;

    old_inode = vfs_lookup(oldpath);
    if (!old_inode)
        return -ENOENT;
    if (inode_is_dir(old_inode))
        return -EPERM;

    if (vfs_lookup(newpath))
        return -EEXIST;

    err = path_parent_name(newpath, parent_path, name);
    if (err)
        return err;

    parent = d_lookup_path(parent_path);
    if (!parent)
        return -ENOENT;

    old_dentry.name[0] = '\0';
    old_dentry.inode = old_inode;
    old_dentry.parent = NULL;
    old_dentry.child = NULL;
    old_dentry.next = NULL;

    str_copy(new_dentry.name, name, sizeof(new_dentry.name));
    new_dentry.inode = NULL;
    new_dentry.parent = parent;
    new_dentry.child = NULL;
    new_dentry.next = NULL;

    return vfs_link(&old_dentry, parent->inode, &new_dentry);
}

long ksys_linkat(int olddfd, const char *oldname, int newdfd,
                 const char *newname, int flags)
{
    char oldpath[PATH_MAX];
    char newpath[PATH_MAX];
    long err;

    (void)olddfd;
    (void)newdfd;

    if (flags)
        return -EINVAL;

    err = getname_from_user(oldpath, oldname);
    if (err)
        return err;

    err = getname_from_user(newpath, newname);
    if (err)
        return err;

    return do_link(oldpath, newpath);
}

static int do_symlink(const char *target, const char *linkpath)
{
    char parent_path[PATH_MAX];
    char name[DNAME_INLINE_LEN];
    struct dentry *parent;
    struct dentry new_dentry;
    int err;

    if (vfs_lookup(linkpath))
        return -EEXIST;

    err = path_parent_name(linkpath, parent_path, name);
    if (err)
        return err;

    parent = d_lookup_path(parent_path);
    if (!parent)
        return -ENOENT;

    str_copy(new_dentry.name, name, sizeof(new_dentry.name));
    new_dentry.inode = NULL;
    new_dentry.parent = parent;
    new_dentry.child = NULL;
    new_dentry.next = NULL;

    return vfs_symlink(target, parent->inode, &new_dentry);
}

long ksys_symlinkat(const char *target, int dfd, const char *linkpath)
{
    char target_buf[PATH_MAX];
    char link_buf[PATH_MAX];
    long n;
    long err;

    (void)dfd;

    if (!target || !linkpath)
        return -EFAULT;

    n = strncpy_from_user(target_buf, target, PATH_MAX);
    if (n < 0)
        return n;
    if (n >= PATH_MAX)
        return -ENAMETOOLONG;

    err = getname_from_user(link_buf, linkpath);
    if (err)
        return err;

    if (link_buf[0] == '/' && link_buf[1] == 'p' && link_buf[2] == 'r' &&
        link_buf[3] == 'o' && link_buf[4] == 'c' && link_buf[5] == '/')
        return -EACCES;

    return do_symlink(target_buf, link_buf);
}

long ksys_readlinkat(int dfd, const char *pathname, char *buf,
                     unsigned long bufsiz)
{
    char path[PATH_MAX];
    char kbuf[PATH_MAX];
    struct inode *inode;
    unsigned long copy_len;
    long len;
    long err;

    (void)dfd;

    if (!current || !current->is_user)
        return -EINVAL;
    if (!pathname || !buf)
        return -EFAULT;
    if (bufsiz == 0)
        return -EINVAL;

    err = getname_from_user(path, pathname);
    if (err)
        return err;

    inode = vfs_lookup(path);
    if (!inode)
        return -ENOENT;

    copy_len = bufsiz;
    if (copy_len > PATH_MAX)
        copy_len = PATH_MAX;

    len = ramfs_readlink(inode, kbuf, copy_len);
    proc_iput(inode);
    if (len < 0)
        return len;

    if (copy_to_user(buf, kbuf, (unsigned long)len))
        return -EFAULT;

    return len;
}

long ksys_chdir(const char *filename)
{
    char path[PATH_MAX];
    struct dentry *d;
    long err;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    d = d_lookup_path(path);
    if (!d)
        return -ENOENT;
    if (!d->inode || !inode_is_dir(d->inode))
        return -ENOTDIR;

    current->cwd = d;
    return 0;
}

long ksys_getcwd(char *user_buf, unsigned long size)
{
    char tmp[PATH_MAX];
    long len;

    if (!current || !current->is_user)
        return -EINVAL;
    if (!user_buf)
        return -EFAULT;
    if (size == 0)
        return -EINVAL;

    len = dentry_path(pwd_dentry(), tmp, PATH_MAX);
    if (len < 0)
        return len;
    if ((unsigned long)len > size)
        return -ERANGE;

    if (copy_to_user(user_buf, tmp, (unsigned long)len))
        return -EFAULT;

    return len;
}

/*
 * utimensat(2) — update timestamps. We do not store times yet; succeed if
 * the path exists so BusyBox touch can create-on-ENOENT via open(O_CREAT).
 */
long ksys_utimensat(int dfd, const char *filename, const struct timespec *times,
                    int flags)
{
    char path[PATH_MAX];
    long err;

    (void)dfd;
    (void)times;
    (void)flags;

    if (!filename)
        return -EFAULT;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    if (!vfs_lookup(path))
        return -ENOENT;

    return 0;
}

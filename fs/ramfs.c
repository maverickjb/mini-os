/*
 * ramfs — in-memory tree; each node embeds a generic struct inode.
 */

#include <linux/ramfs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/dirent.h>
#include <linux/uaccess.h>

struct ramfs_dentry {
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *inode;
    struct ramfs_dentry *next;
};

static struct ramfs_inode root_inode;
static unsigned long next_ino = 1;

static long ramfs_file_read(struct file *file, char *buf, unsigned long count,
                            long *pos)
{
    long n;

    if (!file || !file->inode)
        return -EINVAL;

    n = ramfs_read(file->inode, buf, count, (unsigned long)*pos);
    if (n > 0)
        *pos += n;

    return n;
}

static long ramfs_file_write(struct file *file, const char *buf,
                             unsigned long count, long *pos)
{
    long n;

    if (!file || !file->inode)
        return -EINVAL;

    n = ramfs_write(file->inode, buf, count, (unsigned long)*pos);
    if (n > 0)
        *pos += n;

    return n;
}

struct file_ops ramfs_file_ops = {
    .read = ramfs_file_read,
    .write = ramfs_file_write,
};

static unsigned char ramfs_dtype(int type)
{
    switch (type) {
    case S_IFREG:
        return DT_REG;
    case S_IFDIR:
        return DT_DIR;
    case S_IFCHR:
        return DT_CHR;
    case S_IFIFO:
        return DT_FIFO;
    default:
        return DT_UNKNOWN;
    }
}

static unsigned long ramfs_namelen(const char *s)
{
    unsigned long n = 0;

    while (s[n])
        n++;
    return n;
}

static long ramfs_dir_readdir(struct file *file, void *dirp, unsigned long count)
{
    struct ramfs_inode *ri;
    struct ramfs_dentry *d;
    long index;
    long pos;
    unsigned long written;

    if (!file || !file->inode || !inode_is_dir(file->inode))
        return -ENOTDIR;
    if (!dirp)
        return -EFAULT;
    if (count == 0)
        return 0;

    ri = RAMFS_I(file->inode);
    index = file->f_pos;
    written = 0;
    pos = 0;

    for (d = ri->children; d; d = d->next, pos++) {
        unsigned long namelen;
        unsigned long reclen;
        unsigned long i;
        char kbuf[sizeof(struct linux_dirent64) + RAMFS_NAME_MAX + 8];
        struct linux_dirent64 *de = (struct linux_dirent64 *)kbuf;

        if (pos < index)
            continue;

        namelen = ramfs_namelen(d->name);
        reclen = (offsetof(struct linux_dirent64, d_name) + namelen + 1UL +
                  7UL) &
                 ~7UL;
        if (reclen > sizeof(kbuf))
            return -EINVAL;

        if (written + reclen > count) {
            if (written == 0)
                return -EINVAL;
            break;
        }

        for (i = 0; i < reclen; i++)
            kbuf[i] = 0;

        de->d_ino = d->inode->inode.ino;
        de->d_off = pos + 1;
        de->d_reclen = (unsigned short)reclen;
        de->d_type = ramfs_dtype(d->inode->inode.type);
        for (i = 0; i < namelen; i++)
            de->d_name[i] = d->name[i];
        de->d_name[namelen] = '\0';

        if (copy_to_user((char *)dirp + written, kbuf, reclen))
            return -EFAULT;

        written += reclen;
        file->f_pos = pos + 1;
    }

    return (long)written;
}

struct file_ops ramfs_dir_ops = {
    .readdir = ramfs_dir_readdir,
};

static const struct inode_operations ramfs_dir_inode_ops;

static int ramfs_streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void ramfs_memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
}

static void *ramfs_kmalloc(unsigned long size)
{
    unsigned int order = 0;
    unsigned long bytes = PAGE_SIZE;

    if (size == 0)
        size = 1;

    while (bytes < size && order < 15U) {
        order++;
        bytes <<= 1;
    }

    return alloc_pages((int)order);
}

static void ramfs_kfree(void *ptr, unsigned long size)
{
    unsigned int order = 0;
    unsigned long bytes = PAGE_SIZE;

    if (!ptr)
        return;

    if (size == 0)
        size = 1;

    while (bytes < size && order < 15U) {
        order++;
        bytes <<= 1;
    }

    free_pages(ptr, (int)order);
}

static void ramfs_inode_init(struct ramfs_inode *ri, int type)
{
    ri->inode.ino = next_ino++;
    ri->inode.size = 0;
    ri->inode.type = type;
    ri->inode.i_op = NULL;
    ri->inode.i_fop = NULL;
    if (type == S_IFREG) {
        ri->inode.i_fop = &ramfs_file_ops;
    } else if (type == S_IFDIR) {
        ri->inode.i_op = &ramfs_dir_inode_ops;
        ri->inode.i_fop = &ramfs_dir_ops;
    }
    ri->inode.private_data = NULL;
    ri->children = NULL;
    ri->data = NULL;
    ri->capacity = 0;
}

static struct ramfs_inode *ramfs_alloc_inode(int type)
{
    struct ramfs_inode *ri = ramfs_kmalloc(sizeof(*ri));

    if (!ri)
        return NULL;

    ramfs_inode_init(ri, type);
    return ri;
}

static void ramfs_free_inode(struct ramfs_inode *ri)
{
    struct ramfs_dentry *d;
    struct ramfs_dentry *next;

    if (!ri)
        return;

    if (ri->inode.type == S_IFDIR) {
        for (d = ri->children; d; d = next) {
            next = d->next;
            ramfs_free_inode(d->inode);
            ramfs_kfree(d, sizeof(*d));
        }
    }

    if (ri->data)
        ramfs_kfree(ri->data, ri->capacity);

    if (ri != &root_inode)
        ramfs_kfree(ri, sizeof(*ri));
}

static struct ramfs_dentry *ramfs_find_child(struct ramfs_inode *dir,
                                             const char *name)
{
    struct ramfs_dentry *d;

    if (!dir || dir->inode.type != S_IFDIR)
        return NULL;

    for (d = dir->children; d; d = d->next) {
        if (ramfs_streq(d->name, name))
            return d;
    }

    return NULL;
}

static int ramfs_add_child(struct ramfs_inode *dir, const char *name,
                           struct ramfs_inode *ri)
{
    struct ramfs_dentry *d;

    if (!dir || dir->inode.type != S_IFDIR)
        return -ENOTDIR;

    if (ramfs_find_child(dir, name))
        return -EEXIST;

    d = ramfs_kmalloc(sizeof(*d));
    if (!d)
        return -ENOMEM;

    {
        unsigned long i = 0;

        while (name[i] && i < RAMFS_NAME_MAX) {
            d->name[i] = name[i];
            i++;
        }
        d->name[i] = '\0';
    }

    d->inode = ri;
    d->next = dir->children;
    dir->children = d;
    return 0;
}

static struct ramfs_inode *ramfs_alloc_inode(int type);

static int ramfs_inode_mkdir(struct inode *dir, struct dentry *dentry,
                             umode_t mode)
{
    struct ramfs_inode *parent;
    struct ramfs_inode *ri;
    int err;

    (void)mode;

    if (!dir || !dentry || !dentry->name[0])
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;

    parent = RAMFS_I(dir);
    if (ramfs_find_child(parent, dentry->name))
        return -EEXIST;

    ri = ramfs_alloc_inode(S_IFDIR);
    if (!ri)
        return -ENOMEM;

    err = ramfs_add_child(parent, dentry->name, ri);
    if (err) {
        ramfs_kfree(ri, sizeof(*ri));
        return err;
    }

    dentry->inode = &ri->inode;
    return 0;
}

static int ramfs_remove_child(struct ramfs_inode *dir, const char *name,
                              struct ramfs_inode **out)
{
    struct ramfs_dentry **prev = &dir->children;
    struct ramfs_dentry *d;

    if (!dir || dir->inode.type != S_IFDIR)
        return -ENOTDIR;

    while (*prev) {
        d = *prev;
        if (ramfs_streq(d->name, name)) {
            *prev = d->next;
            if (out)
                *out = d->inode;
            ramfs_kfree(d, sizeof(*d));
            return 0;
        }
        prev = &d->next;
    }

    return -ENOENT;
}

static int ramfs_inode_unlink(struct inode *dir, struct dentry *dentry)
{
    struct ramfs_inode *parent;
    struct ramfs_inode *ri;
    int err;

    if (!dir || !dentry || !dentry->name[0])
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;

    parent = RAMFS_I(dir);
    err = ramfs_remove_child(parent, dentry->name, &ri);
    if (err)
        return err;

    if (ri->inode.type == S_IFDIR) {
        ramfs_add_child(parent, dentry->name, ri);
        return -EISDIR;
    }

    ramfs_free_inode(ri);
    dentry->inode = NULL;
    return 0;
}

static int ramfs_inode_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct ramfs_inode *parent;
    struct ramfs_inode *ri;
    int err;

    if (!dir || !dentry || !dentry->name[0])
        return -EINVAL;
    if (!inode_is_dir(dir))
        return -ENOTDIR;

    parent = RAMFS_I(dir);
    err = ramfs_remove_child(parent, dentry->name, &ri);
    if (err)
        return err;

    if (ri->inode.type != S_IFDIR) {
        ramfs_add_child(parent, dentry->name, ri);
        return -ENOTDIR;
    }

    if (ri->children) {
        ramfs_add_child(parent, dentry->name, ri);
        return -ENOTEMPTY;
    }

    ramfs_free_inode(ri);
    dentry->inode = NULL;
    return 0;
}

static const struct inode_operations ramfs_dir_inode_ops = {
    .mkdir = ramfs_inode_mkdir,
    .unlink = ramfs_inode_unlink,
    .rmdir = ramfs_inode_rmdir,
};

static const char *ramfs_skip_slash(const char *path)
{
    while (*path == '/')
        path++;
    return path;
}

static struct ramfs_inode *ramfs_lookup_at(struct ramfs_inode *dir,
                                           const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_dentry *d;
    unsigned long i;

    path = ramfs_skip_slash(path);
    if (!*path)
        return dir;

    for (;;) {
        i = 0;
        while (path[i] && path[i] != '/' && i < RAMFS_NAME_MAX)
            i++;

        if (i == 0)
            return NULL;

        {
            unsigned long j;

            for (j = 0; j < i; j++)
                name[j] = path[j];
            name[i] = '\0';
        }

        d = ramfs_find_child(dir, name);
        if (!d)
            return NULL;

        path += i;
        path = ramfs_skip_slash(path);
        if (!*path)
            return d->inode;

        if (d->inode->inode.type != S_IFDIR)
            return NULL;

        dir = d->inode;
    }
}

static struct ramfs_inode *ramfs_lookup_parent(const char *path, char *name_out)
{
    struct ramfs_inode *dir = &root_inode;
    struct ramfs_dentry *d;
    const char *p = ramfs_skip_slash(path);
    const char *last = NULL;
    unsigned long i;

    if (!*p)
        return NULL;

    while (*p) {
        while (*p == '/')
            p++;

        if (!*p)
            break;

        last = p;
        i = 0;
        while (p[i] && p[i] != '/')
            i++;

        if (p[i]) {
            char component[RAMFS_NAME_MAX + 1];
            unsigned long j;

            for (j = 0; j < i && j < RAMFS_NAME_MAX; j++)
                component[j] = p[j];
            component[j] = '\0';

            d = ramfs_find_child(dir, component);
            if (!d)
                return NULL;
            if (d->inode->inode.type != S_IFDIR)
                return NULL;
            dir = d->inode;
            p += i;
        } else {
            break;
        }
    }

    if (!last)
        return NULL;

    i = 0;
    while (last[i] && last[i] != '/')
        i++;

    if (i == 0 || i > RAMFS_NAME_MAX)
        return NULL;

    {
        unsigned long j;

        for (j = 0; j < i; j++)
            name_out[j] = last[j];
        name_out[i] = '\0';
    }

    return dir;
}

void ramfs_init(void)
{
    next_ino = 1;
    ramfs_inode_init(&root_inode, S_IFDIR);
}

struct inode *ramfs_root(void)
{
    return &root_inode.inode;
}

struct inode *ramfs_lookup(const char *path)
{
    struct ramfs_inode *ri;

    if (!path || path[0] != '/')
        return NULL;

    ri = ramfs_lookup_at(&root_inode, path);
    if (!ri)
        return NULL;

    return &ri->inode;
}

int ramfs_mkdir(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct dentry dentry;
    unsigned long i;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    for (i = 0; i < sizeof(dentry.name); i++)
        dentry.name[i] = 0;
    for (i = 0; name[i] && i + 1 < sizeof(dentry.name); i++)
        dentry.name[i] = name[i];
    dentry.name[i] = '\0';
    dentry.inode = NULL;
    dentry.parent = NULL;
    dentry.next = NULL;

    return vfs_mkdir(&parent->inode, &dentry, 0755);
}

int ramfs_create(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct ramfs_inode *ri;
    int err;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    if (ramfs_find_child(parent, name))
        return -EEXIST;

    ri = ramfs_alloc_inode(S_IFREG);
    if (!ri)
        return -ENOMEM;

    err = ramfs_add_child(parent, name, ri);
    if (err) {
        ramfs_kfree(ri, sizeof(*ri));
        return err;
    }

    return 0;
}

int ramfs_unlink(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct dentry dentry;
    unsigned long i;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    for (i = 0; i < sizeof(dentry.name); i++)
        dentry.name[i] = 0;
    for (i = 0; name[i] && i + 1 < sizeof(dentry.name); i++)
        dentry.name[i] = name[i];
    dentry.name[i] = '\0';
    dentry.inode = NULL;
    dentry.parent = NULL;
    dentry.next = NULL;

    return vfs_unlink(&parent->inode, &dentry);
}

int ramfs_rmdir(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct ramfs_dentry *child;
    struct dentry dentry;
    unsigned long i;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    child = ramfs_find_child(parent, name);
    if (!child)
        return -ENOENT;

    for (i = 0; i < sizeof(dentry.name); i++)
        dentry.name[i] = 0;
    for (i = 0; name[i] && i + 1 < sizeof(dentry.name); i++)
        dentry.name[i] = name[i];
    dentry.name[i] = '\0';
    dentry.inode = &child->inode->inode;
    dentry.parent = NULL;
    dentry.next = NULL;

    return vfs_rmdir(&parent->inode, &dentry);
}

static int ramfs_ensure_capacity(struct ramfs_inode *ri, unsigned long need)
{
    void *new_data;
    unsigned long new_cap;

    if (ri->inode.type != S_IFREG)
        return -EISDIR;

    if (need <= ri->capacity)
        return 0;

    new_cap = ri->capacity ? ri->capacity : PAGE_SIZE;
    while (new_cap < need)
        new_cap <<= 1;

    new_data = ramfs_kmalloc(new_cap);
    if (!new_data)
        return -ENOMEM;

    if (ri->data && ri->inode.size)
        ramfs_memcpy(new_data, ri->data, ri->inode.size);

    if (ri->data)
        ramfs_kfree(ri->data, ri->capacity);

    ri->data = new_data;
    ri->capacity = new_cap;
    return 0;
}

long ramfs_read(struct inode *inode, void *buf, unsigned long len,
                unsigned long offset)
{
    struct ramfs_inode *ri;
    unsigned long avail;

    if (!inode || !buf)
        return -EINVAL;
    if (inode->type != S_IFREG)
        return -EISDIR;

    ri = RAMFS_I(inode);

    if (offset >= inode->size)
        return 0;

    avail = inode->size - offset;
    if (len > avail)
        len = avail;

    if (len && ri->data)
        ramfs_memcpy(buf, (unsigned char *)ri->data + offset, len);

    return (long)len;
}

long ramfs_write(struct inode *inode, const void *buf, unsigned long len,
                 unsigned long offset)
{
    struct ramfs_inode *ri;
    unsigned long end;
    int err;

    if (!inode || !buf)
        return -EINVAL;
    if (inode->type != S_IFREG)
        return -EISDIR;

    ri = RAMFS_I(inode);
    end = offset + len;
    if (end < offset)
        return -EINVAL;

    err = ramfs_ensure_capacity(ri, end);
    if (err)
        return err;

    if (len)
        ramfs_memcpy((unsigned char *)ri->data + offset, buf, len);

    if (end > inode->size)
        inode->size = end;

    return (long)len;
}

int ramfs_truncate(struct inode *inode, unsigned long size)
{
    struct ramfs_inode *ri;

    if (!inode)
        return -EINVAL;
    if (inode->type != S_IFREG)
        return -EISDIR;

    ri = RAMFS_I(inode);
    if (size > ri->capacity)
        return -EINVAL;

    inode->size = size;
    return 0;
}

int ramfs_readdir(struct inode *dir, ramfs_readdir_fn fn, void *arg)
{
    struct ramfs_inode *ri;
    struct ramfs_dentry *d;

    if (!dir || !fn)
        return -EINVAL;
    if (dir->type != S_IFDIR)
        return -ENOTDIR;

    ri = RAMFS_I(dir);
    for (d = ri->children; d; d = d->next)
        fn(d->name, &d->inode->inode, arg);

    return 0;
}

const void *ramfs_data(struct inode *inode)
{
    if (!inode || inode->type != S_IFREG)
        return NULL;

    return RAMFS_I(inode)->data;
}

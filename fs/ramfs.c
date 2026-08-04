/*
 * ramfs — purely in-memory tree of files and directories.
 */

#include "ramfs.h"
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/stddef.h>

struct ramfs_dentry {
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *inode;
    struct ramfs_dentry *next;
};

struct ramfs_inode {
    unsigned int mode;
    struct ramfs_dentry *children;
    void *data;
    unsigned long size;
    unsigned long capacity;
};

static struct ramfs_inode root_inode;

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

static struct ramfs_inode *ramfs_alloc_inode(unsigned int mode)
{
    struct ramfs_inode *inode = ramfs_kmalloc(sizeof(*inode));

    if (!inode)
        return NULL;

    inode->mode = mode;
    inode->children = NULL;
    inode->data = NULL;
    inode->size = 0;
    inode->capacity = 0;
    return inode;
}

static void ramfs_free_inode(struct ramfs_inode *inode)
{
    struct ramfs_dentry *d;
    struct ramfs_dentry *next;

    if (!inode)
        return;

    if (inode->mode & RAMFS_S_IFDIR) {
        for (d = inode->children; d; d = next) {
            next = d->next;
            ramfs_free_inode(d->inode);
            ramfs_kfree(d, sizeof(*d));
        }
    }

    if (inode->data)
        ramfs_kfree(inode->data, inode->capacity);

    if (inode != &root_inode)
        ramfs_kfree(inode, sizeof(*inode));
}

static struct ramfs_dentry *ramfs_find_child(struct ramfs_inode *dir,
                                             const char *name)
{
    struct ramfs_dentry *d;

    if (!dir || !(dir->mode & RAMFS_S_IFDIR))
        return NULL;

    for (d = dir->children; d; d = d->next) {
        if (ramfs_streq(d->name, name))
            return d;
    }

    return NULL;
}

static int ramfs_add_child(struct ramfs_inode *dir, const char *name,
                           struct ramfs_inode *inode)
{
    struct ramfs_dentry *d;

    if (!dir || !(dir->mode & RAMFS_S_IFDIR))
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

    d->inode = inode;
    d->next = dir->children;
    dir->children = d;
    return 0;
}

static int ramfs_remove_child(struct ramfs_inode *dir, const char *name,
                              struct ramfs_inode **out)
{
    struct ramfs_dentry **prev = &dir->children;
    struct ramfs_dentry *d;

    if (!dir || !(dir->mode & RAMFS_S_IFDIR))
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

        if (!(d->inode->mode & RAMFS_S_IFDIR))
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
            if (!(d->inode->mode & RAMFS_S_IFDIR))
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
    root_inode.mode = RAMFS_S_IFDIR;
    root_inode.children = NULL;
    root_inode.data = NULL;
    root_inode.size = 0;
    root_inode.capacity = 0;
}

struct ramfs_inode *ramfs_root(void)
{
    return &root_inode;
}

struct ramfs_inode *ramfs_lookup(const char *path)
{
    if (!path || path[0] != '/')
        return NULL;

    return ramfs_lookup_at(&root_inode, path);
}

int ramfs_mkdir(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct ramfs_inode *inode;
    int err;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    if (ramfs_find_child(parent, name))
        return -EEXIST;

    inode = ramfs_alloc_inode(RAMFS_S_IFDIR);
    if (!inode)
        return -ENOMEM;

    err = ramfs_add_child(parent, name, inode);
    if (err) {
        ramfs_kfree(inode, sizeof(*inode));
        return err;
    }

    return 0;
}

int ramfs_create(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct ramfs_inode *inode;
    int err;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    if (ramfs_find_child(parent, name))
        return -EEXIST;

    inode = ramfs_alloc_inode(RAMFS_S_IFREG);
    if (!inode)
        return -ENOMEM;

    err = ramfs_add_child(parent, name, inode);
    if (err) {
        ramfs_kfree(inode, sizeof(*inode));
        return err;
    }

    return 0;
}

int ramfs_unlink(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct ramfs_inode *inode;
    int err;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    err = ramfs_remove_child(parent, name, &inode);
    if (err)
        return err;

    if (inode->mode & RAMFS_S_IFDIR) {
        ramfs_add_child(parent, name, inode);
        return -EISDIR;
    }

    ramfs_free_inode(inode);
    return 0;
}

int ramfs_rmdir(const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    struct ramfs_inode *parent;
    struct ramfs_inode *inode;
    int err;

    if (!path || path[0] != '/')
        return -EINVAL;

    parent = ramfs_lookup_parent(path, name);
    if (!parent)
        return -ENOENT;

    err = ramfs_remove_child(parent, name, &inode);
    if (err)
        return err;

    if (!(inode->mode & RAMFS_S_IFDIR)) {
        ramfs_add_child(parent, name, inode);
        return -ENOTDIR;
    }

    if (inode->children) {
        ramfs_add_child(parent, name, inode);
        return -ENOTEMPTY;
    }

    ramfs_free_inode(inode);
    return 0;
}

static int ramfs_ensure_capacity(struct ramfs_inode *inode, unsigned long need)
{
    void *new_data;
    unsigned long new_cap;

    if (!(inode->mode & RAMFS_S_IFREG))
        return -EISDIR;

    if (need <= inode->capacity)
        return 0;

    new_cap = inode->capacity ? inode->capacity : PAGE_SIZE;
    while (new_cap < need)
        new_cap <<= 1;

    new_data = ramfs_kmalloc(new_cap);
    if (!new_data)
        return -ENOMEM;

    if (inode->data && inode->size)
        ramfs_memcpy(new_data, inode->data, inode->size);

    if (inode->data)
        ramfs_kfree(inode->data, inode->capacity);

    inode->data = new_data;
    inode->capacity = new_cap;
    return 0;
}

long ramfs_read(struct ramfs_inode *inode, void *buf, unsigned long len,
                unsigned long offset)
{
    unsigned long avail;

    if (!inode || !buf)
        return -EINVAL;
    if (!(inode->mode & RAMFS_S_IFREG))
        return -EISDIR;

    if (offset >= inode->size)
        return 0;

    avail = inode->size - offset;
    if (len > avail)
        len = avail;

    if (len && inode->data)
        ramfs_memcpy(buf, (unsigned char *)inode->data + offset, len);

    return (long)len;
}

long ramfs_write(struct ramfs_inode *inode, const void *buf, unsigned long len,
                 unsigned long offset)
{
    unsigned long end;
    int err;

    if (!inode || !buf)
        return -EINVAL;
    if (!(inode->mode & RAMFS_S_IFREG))
        return -EISDIR;

    end = offset + len;
    if (end < offset)
        return -EINVAL;

    err = ramfs_ensure_capacity(inode, end);
    if (err)
        return err;

    if (len)
        ramfs_memcpy((unsigned char *)inode->data + offset, buf, len);

    if (end > inode->size)
        inode->size = end;

    return (long)len;
}

int ramfs_truncate(struct ramfs_inode *inode, unsigned long size)
{
    if (!inode)
        return -EINVAL;
    if (!(inode->mode & RAMFS_S_IFREG))
        return -EISDIR;

    if (size > inode->capacity)
        return -EINVAL;

    inode->size = size;
    return 0;
}

int ramfs_readdir(struct ramfs_inode *dir, ramfs_readdir_fn fn, void *arg)
{
    struct ramfs_dentry *d;

    if (!dir || !fn)
        return -EINVAL;
    if (!(dir->mode & RAMFS_S_IFDIR))
        return -ENOTDIR;

    for (d = dir->children; d; d = d->next)
        fn(d->name, d->inode, arg);

    return 0;
}

int ramfs_is_dir(const struct ramfs_inode *inode)
{
    return inode && (inode->mode & RAMFS_S_IFDIR);
}

int ramfs_is_reg(const struct ramfs_inode *inode)
{
    return inode && (inode->mode & RAMFS_S_IFREG);
}

unsigned long ramfs_size(const struct ramfs_inode *inode)
{
    return inode ? inode->size : 0;
}

const void *ramfs_data(const struct ramfs_inode *inode)
{
    if (!inode || !(inode->mode & RAMFS_S_IFREG))
        return NULL;

    return inode->data;
}

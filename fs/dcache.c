/*
 * Minimal VFS dentry tree for cwd / getcwd.
 */

#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/ramfs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/string.h>

static struct dentry root_dentry;

void dcache_init(void)
{
    root_dentry.name[0] = '\0';
    root_dentry.inode = ramfs_root();
    root_dentry.parent = NULL;
    root_dentry.child = NULL;
    root_dentry.next = NULL;
}

struct dentry *d_root(void)
{
    return &root_dentry;
}

static struct dentry *d_find_child(struct dentry *parent, const char *name)
{
    struct dentry *d;

    if (!parent || !name)
        return NULL;

    for (d = parent->child; d; d = d->next) {
        if (strcmp(d->name, name) == 0)
            return d;
    }
    return NULL;
}

struct dentry *d_alloc(struct dentry *parent, const char *name)
{
    struct dentry *d;

    if (!parent || !name || !name[0])
        return NULL;

    d = kmalloc(sizeof(*d));
    if (!d)
        return NULL;

    memset(d, 0, sizeof(*d));
    strscpy(d->name, name, sizeof(d->name));
    d->parent = parent;
    d->inode = NULL;
    d->child = NULL;
    d->next = NULL;
    return d;
}

void d_add(struct dentry *dentry)
{
    if (!dentry || !dentry->parent)
        return;

    dentry->next = dentry->parent->child;
    dentry->parent->child = dentry;
}

static void d_unlink(struct dentry *dentry)
{
    struct dentry **prev;

    if (!dentry || !dentry->parent)
        return;

    prev = &dentry->parent->child;
    while (*prev) {
        if (*prev == dentry) {
            *prev = dentry->next;
            dentry->next = NULL;
            return;
        }
        prev = &(*prev)->next;
    }
}

static void cwd_fix(struct dentry *old, struct dentry *neu)
{
    struct list_head *pos;
    struct task_struct *t;
    unsigned long flags;

    if (current && current->cwd == old)
        current->cwd = neu;

    task_list_lock_irqsave(&flags);
    for_each_task(pos, t) {
        if (t->cwd == old)
            t->cwd = neu;
    }
    task_list_unlock_irqrestore(flags);
}

void d_drop(struct dentry *dentry)
{
    if (!dentry || dentry == &root_dentry)
        return;

    cwd_fix(dentry, dentry->parent ? dentry->parent : &root_dentry);
    d_unlink(dentry);
    kfree(dentry);
}

/*
 * Walk an absolute path, lazily attaching VFS dentries for ramfs nodes.
 */
struct dentry *d_lookup_path(const char *path)
{
    struct dentry *d = &root_dentry;
    const char *p;

    if (!path || path[0] != '/')
        return NULL;

    p = path;
    while (*p == '/')
        p++;

    if (!*p)
        return &root_dentry;

    while (*p) {
        char name[DNAME_INLINE_LEN];
        unsigned long i = 0;
        struct dentry *child;
        struct inode *inode;

        while (*p == '/')
            p++;
        if (!*p)
            break;

        while (p[i] && p[i] != '/' && i + 1 < sizeof(name)) {
            name[i] = p[i];
            i++;
        }
        if (!p[i] || p[i] == '/')
            name[i] = '\0';
        else
            return NULL; /* component too long */
        p += i;

        /* "." — stay on the current dentry */
        if (name[0] == '.' && name[1] == '\0')
            continue;

        /* ".." — parent dentry; root stays at root */
        if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
            if (d->parent)
                d = d->parent;
            continue;
        }

        child = d_find_child(d, name);
        if (!child) {
            inode = ramfs_lookup_child(d->inode, name);
            if (!inode)
                return NULL;
            child = d_alloc(d, name);
            if (!child)
                return NULL;
            child->inode = inode;
            d_add(child);
        }
        d = child;
    }

    return d;
}

long dentry_path(struct dentry *dentry, char *buf, unsigned long size)
{
    char tmp[PATH_MAX];
    unsigned long pos = PATH_MAX - 1;
    struct dentry *d = dentry ? dentry : &root_dentry;
    unsigned long len;

    if (!buf || size == 0)
        return -EINVAL;

    tmp[pos] = '\0';

    /* Root: parent is NULL */
    if (!d->parent) {
        if (size < 2)
            return -ERANGE;
        buf[0] = '/';
        buf[1] = '\0';
        return 2;
    }

    while (d && d->parent) {
        unsigned long nlen = strlen(d->name);

        if (pos < nlen + 1)
            return -ENAMETOOLONG;

        pos -= nlen;
        {
            unsigned long i;

            for (i = 0; i < nlen; i++)
                tmp[pos + i] = d->name[i];
        }
        pos--;
        tmp[pos] = '/';
        d = d->parent;
    }

    len = PATH_MAX - pos; /* includes NUL */
    if (len > size)
        return -ERANGE;

    {
        unsigned long i;

        for (i = 0; i < len; i++)
            buf[i] = tmp[pos + i];
    }

    return (long)len;
}

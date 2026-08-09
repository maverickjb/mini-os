#ifndef _LINUX_DCACHE_H
#define _LINUX_DCACHE_H

struct inode;

#define DNAME_INLINE_LEN 256

struct dentry {
    char name[DNAME_INLINE_LEN];
    struct inode *inode;
    struct dentry *parent;
    struct dentry *child; /* first child */
    struct dentry *next;  /* next sibling */
};

void dcache_init(void);
struct dentry *d_root(void);
struct dentry *d_lookup_path(const char *path);
struct dentry *d_alloc(struct dentry *parent, const char *name);
void d_add(struct dentry *dentry);
void d_drop(struct dentry *dentry);
/* Absolute path into buf; returns length including NUL, or -errno. */
long dentry_path(struct dentry *dentry, char *buf, unsigned long size);

#endif /* _LINUX_DCACHE_H */

#ifndef _LINUX_DCACHE_H
#define _LINUX_DCACHE_H

struct inode;

#define DNAME_INLINE_LEN 256

struct dentry {
    char name[DNAME_INLINE_LEN];
    struct inode *inode;
    struct dentry *parent;
    struct dentry *next;
};

#endif /* _LINUX_DCACHE_H */

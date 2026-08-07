#ifndef _LINUX_DCACHE_H
#define _LINUX_DCACHE_H

#include <linux/fs.h>

struct dentry {
    char name[32];
    struct inode *inode;
    struct dentry *parent;
    struct dentry *next;
};

#endif /* _LINUX_DCACHE_H */

#ifndef _LINUX_NAMEI_H
#define _LINUX_NAMEI_H

#include <linux/fs.h>

struct dentry;

struct inode *vfs_lookup(const char *path);
int vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

#endif /* _LINUX_NAMEI_H */

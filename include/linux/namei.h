#ifndef _LINUX_NAMEI_H
#define _LINUX_NAMEI_H

#include <linux/fs.h>

struct dentry;

struct inode *vfs_lookup(const char *path);
int vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int vfs_unlink(struct inode *dir, struct dentry *dentry);
int vfs_rmdir(struct inode *dir, struct dentry *dentry);
int vfs_link(struct dentry *old_dentry, struct inode *dir,
             struct dentry *new_dentry);
int vfs_symlink(const char *target, struct inode *dir, struct dentry *dentry);

/* Copy filename from user and resolve against current->cwd to an absolute path. */
long getname_from_user(char *buf, const char *filename);

#endif /* _LINUX_NAMEI_H */

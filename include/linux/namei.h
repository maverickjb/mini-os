#ifndef _LINUX_NAMEI_H
#define _LINUX_NAMEI_H

struct inode;

struct inode *vfs_lookup(const char *path);

#endif /* _LINUX_NAMEI_H */

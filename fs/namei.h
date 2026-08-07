#ifndef NAMEI_H
#define NAMEI_H

struct inode;

struct inode *vfs_lookup(const char *path);

#endif

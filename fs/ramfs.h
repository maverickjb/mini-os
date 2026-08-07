#ifndef RAMFS_H
#define RAMFS_H

/*
 * In-memory filesystem. Each ramfs_inode embeds a generic VFS inode.
 */

#include <linux/fs.h>
#include <linux/stddef.h>

#define RAMFS_NAME_MAX  255

struct ramfs_dentry;

struct ramfs_inode {
    struct inode inode;
    struct ramfs_dentry *children;
    void *data;
    unsigned long capacity;
};

static inline struct ramfs_inode *RAMFS_I(const struct inode *inode)
{
    return container_of(inode, struct ramfs_inode, inode);
}

typedef void (*ramfs_readdir_fn)(const char *name, struct inode *inode,
                                 void *arg);

void ramfs_init(void);
struct inode *ramfs_root(void);

struct inode *ramfs_lookup(const char *path);
int ramfs_mkdir(const char *path);
int ramfs_create(const char *path);
int ramfs_unlink(const char *path);
int ramfs_rmdir(const char *path);

long ramfs_read(struct inode *inode, void *buf, unsigned long len,
                unsigned long offset);
long ramfs_write(struct inode *inode, const void *buf, unsigned long len,
                 unsigned long offset);
int ramfs_truncate(struct inode *inode, unsigned long size);

int ramfs_readdir(struct inode *dir, ramfs_readdir_fn fn, void *arg);

const void *ramfs_data(struct inode *inode);

extern struct file_ops ramfs_file_ops;

#endif

#ifndef _LINUX_RAMFS_H
#define _LINUX_RAMFS_H

/*
 * In-memory filesystem. Each ramfs_inode embeds a generic VFS inode.
 */

#include <linux/fs.h>
#include <linux/stddef.h>

#define RAMFS_NAME_MAX  255

struct ramfs_dentry;

struct ramfs_inode {
    struct inode inode;
    struct ramfs_inode *parent;
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
struct inode *ramfs_lookup_child(struct inode *dir, const char *name);
int ramfs_mkdir(const char *path);
int ramfs_create(const char *path);
int ramfs_symlink(const char *path, const char *target);
int ramfs_unlink(const char *path);
int ramfs_rmdir(const char *path);

long ramfs_read(struct inode *inode, void *buf, unsigned long len,
                unsigned long offset);
long ramfs_write(struct inode *inode, const void *buf, unsigned long len,
                 unsigned long offset);
int ramfs_truncate(struct inode *inode, unsigned long size);

int ramfs_readdir(struct inode *dir, ramfs_readdir_fn fn, void *arg);

const void *ramfs_data(struct inode *inode);
long ramfs_readlink(struct inode *inode, char *buf, unsigned long bufsiz);

extern struct file_ops ramfs_file_ops;
extern struct file_ops ramfs_dir_ops;

#endif /* _LINUX_RAMFS_H */

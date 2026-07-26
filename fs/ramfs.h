#ifndef RAMFS_H
#define RAMFS_H

/*
 * Simple in-memory filesystem (Linux ramfs-like, no VFS).
 *
 * All paths are absolute from the single mount root, e.g. "/tmp/foo".
 */

#define RAMFS_NAME_MAX  255

#define RAMFS_S_IFDIR   0040000U
#define RAMFS_S_IFREG   0100000U

struct ramfs_inode;

typedef void (*ramfs_readdir_fn)(const char *name, struct ramfs_inode *inode, void *arg);

void ramfs_init(void);
struct ramfs_inode *ramfs_root(void);

struct ramfs_inode *ramfs_lookup(const char *path);
int ramfs_mkdir(const char *path);
int ramfs_create(const char *path);
int ramfs_unlink(const char *path);
int ramfs_rmdir(const char *path);

long ramfs_read(struct ramfs_inode *inode, void *buf, unsigned long len,
                unsigned long offset);
long ramfs_write(struct ramfs_inode *inode, const void *buf, unsigned long len,
                 unsigned long offset);
int ramfs_truncate(struct ramfs_inode *inode, unsigned long size);

int ramfs_readdir(struct ramfs_inode *dir, ramfs_readdir_fn fn, void *arg);

int ramfs_is_dir(const struct ramfs_inode *inode);
int ramfs_is_reg(const struct ramfs_inode *inode);
unsigned long ramfs_size(const struct ramfs_inode *inode);

#endif

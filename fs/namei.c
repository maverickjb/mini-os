/*
 * Pathname lookup — VFS entry that currently resolves via ramfs.
 */

#include <linux/fs.h>
#include <linux/stddef.h>

#include <linux/namei.h>
#include <linux/ramfs.h>

struct inode *vfs_lookup(const char *path)
{
    return ramfs_lookup(path);
}

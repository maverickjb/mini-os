/*
 * /dev/null — discard writes, read returns EOF (0).
 */

#include <linux/devnull.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/ramfs.h>
#include <linux/errno.h>

static long devnull_read(struct file *file, char *buf, unsigned long count,
                         long *pos)
{
    (void)file;
    (void)buf;
    (void)count;
    (void)pos;
    return 0;
}

static long devnull_write(struct file *file, const char *buf,
                          unsigned long count, long *pos)
{
    (void)file;
    (void)buf;

    if (pos)
        *pos += (long)count;
    return (long)count;
}

static struct file_ops devnull_fops = {
    .read = devnull_read,
    .write = devnull_write,
};

int devnull_is_path(const char *path)
{
    return path && strcmp(path, "/dev/null") == 0;
}

int devnull_file_is(const struct file *file)
{
    return file && file->f_op == &devnull_fops;
}

struct file *devnull_open(int flags)
{
    struct file *file;

    file = alloc_file();
    if (!file)
        return NULL;

    file->f_op = &devnull_fops;
    file->f_flags = flags;
    return file;
}

void devnull_fill_stat(struct stat *st)
{
    if (!st)
        return;

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR | 0666;
    st->st_rdev = (1UL << 8) | 3UL; /* Linux /dev/null is 1:3 */
    st->st_nlink = 1;
    st->st_blksize = 1024;
}

void devnull_init(void)
{
    int err;

    err = ramfs_mkdir("/dev");
    if (err && err != -EEXIST)
        return;
}

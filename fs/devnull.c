/*
 * /dev/null — discard writes, read returns EOF (0).
 */

#include <linux/devnull.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/chrdev.h>
#include <linux/stat.h>
#include <linux/string.h>

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
    st->st_rdev = MKDEV(MEM_MAJOR, 3);
    st->st_nlink = 1;
    st->st_blksize = 1024;
}

/*
 * /dev/console — system console (UART-backed, always openable).
 */

#include <linux/devconsole.h>
#include <linux/dev.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/tty.h>

int devconsole_is_path(const char *path)
{
    return path && strcmp(path, "/dev/console") == 0;
}

int devconsole_file_is(const struct file *file)
{
    return file && file->f_op == &tty_fops &&
           file->private_data == DEV_FD_CONSOLE;
}

struct file *devconsole_open(int flags)
{
    struct file *file;

    file = alloc_file();
    if (!file)
        return NULL;

    file->f_op = &tty_fops;
    file->f_flags = flags;
    file->private_data = DEV_FD_CONSOLE;
    return file;
}

void devconsole_fill_stat(struct stat *st)
{
    if (!st)
        return;

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR | 0600;
    st->st_rdev = (5UL << 8) | 1UL; /* Linux /dev/console is 5:1 */
    st->st_nlink = 1;
    st->st_blksize = 1024;
}

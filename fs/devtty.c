/*
 * /dev/tty — controlling console TTY (UART-backed).
 */

#include <linux/devtty.h>
#include <linux/fs.h>
#include <linux/sched/task.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/errno.h>

int devtty_is_path(const char *path)
{
    return path && strcmp(path, "/dev/tty") == 0;
}

int devtty_file_is(const struct file *file)
{
    return file && file->f_op == &tty_fops;
}

struct file *devtty_open(int flags)
{
    struct task_struct *task = current;
    struct file *file;

    if (!task || !task->sid || !tty0.session_id ||
        task->sid != tty0.session_id)
        return NULL;

    file = alloc_file();
    if (!file)
        return NULL;

    file->f_op = &tty_fops;
    file->f_flags = flags;
    return file;
}

void devtty_fill_stat(struct stat *st)
{
    if (!st)
        return;

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR | 0666;
    st->st_rdev = (5UL << 8) | 0UL; /* Linux /dev/tty is 5:0 */
    st->st_nlink = 1;
    st->st_blksize = 1024;
}

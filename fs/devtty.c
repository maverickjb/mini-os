/*
 * /dev/tty — controlling console TTY (UART-backed).
 */

#include <linux/devtty.h>
#include <linux/dev.h>
#include <linux/fs.h>
#include <linux/sched/task.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/kdev_t.h>
#include <linux/chrdev.h>

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
    file->private_data = DEV_FD_TTY;
    return file;
}

int devtty_file_is(const struct file *file)
{
    return file && file->f_op == &tty_fops &&
           file->private_data == DEV_FD_TTY;
}

void devtty_fill_stat(struct stat *st)
{
    if (!st)
        return;

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR | 0666;
    st->st_rdev = MKDEV(TTYAUX_MAJOR, 0);
    st->st_nlink = 1;
    st->st_blksize = 1024;
}

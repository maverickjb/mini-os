#ifndef _LINUX_DEV_H
#define _LINUX_DEV_H

#include <linux/fs.h>

struct stat;

int dev_is_path(const char *path);
struct file *dev_open_path(const char *path, int flags);
int dev_fill_stat_path(const char *path, struct stat *st);

#define DEV_FD_TTY      ((void *)1)
#define DEV_FD_CONSOLE  ((void *)2)

#endif /* _LINUX_DEV_H */

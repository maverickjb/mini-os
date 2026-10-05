#ifndef _LINUX_DEVTTY_H
#define _LINUX_DEVTTY_H

#include <linux/fs.h>

struct stat;

struct file *devtty_open(int flags);
int devtty_file_is(const struct file *file);
void devtty_fill_stat(struct stat *st);

#endif /* _LINUX_DEVTTY_H */

#ifndef _LINUX_DEVNULL_H
#define _LINUX_DEVNULL_H

#include <linux/fs.h>

struct stat;

struct file *devnull_open(int flags);
int devnull_file_is(const struct file *file);
void devnull_fill_stat(struct stat *st);

#endif /* _LINUX_DEVNULL_H */

#ifndef _LINUX_DEVNULL_H
#define _LINUX_DEVNULL_H

#include <linux/fs.h>

struct stat;

int devnull_is_path(const char *path);
struct file *devnull_open(int flags);
int devnull_file_is(const struct file *file);
void devnull_fill_stat(struct stat *st);
void devnull_init(void);

#endif /* _LINUX_DEVNULL_H */

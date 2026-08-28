#ifndef _LINUX_DEVCONSOLE_H
#define _LINUX_DEVCONSOLE_H

#include <linux/fs.h>

struct stat;

int devconsole_is_path(const char *path);
struct file *devconsole_open(int flags);
int devconsole_file_is(const struct file *file);
void devconsole_fill_stat(struct stat *st);

#endif /* _LINUX_DEVCONSOLE_H */

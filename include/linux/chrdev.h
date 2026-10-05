#ifndef _LINUX_CHRDEV_H
#define _LINUX_CHRDEV_H

#include <linux/types.h>
#include <linux/fs.h>

#define MEM_MAJOR       1       /* /dev/null, … */
#define TTYAUX_MAJOR    5       /* /dev/tty, /dev/console */

typedef int (*chrdev_open_t)(dev_t rdev, int flags, struct file **out);

int register_chrdev(unsigned int major, const char *name, chrdev_open_t open);
int chrdev_open(dev_t rdev, int flags, struct file **out);

#endif /* _LINUX_CHRDEV_H */

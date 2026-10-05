#ifndef _LINUX_DEV_H
#define _LINUX_DEV_H

#include <linux/fs.h>

#define DEV_FD_TTY      ((void *)1)
#define DEV_FD_CONSOLE  ((void *)2)

void dev_init(void);

#endif /* _LINUX_DEV_H */

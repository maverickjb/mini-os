#ifndef _LINUX_KDEV_T_H
#define _LINUX_KDEV_T_H

#include <linux/types.h>

/*
 * Old-style 8:8 major/minor encoding (matches Linux for these nodes:
 *   /dev/null    1:3
 *   /dev/tty     5:0
 *   /dev/console 5:1
 */
#define MINORBITS       8
#define MINORMASK       ((1U << MINORBITS) - 1)

#define MAJOR(dev)      ((unsigned int)((dev) >> MINORBITS))
#define MINOR(dev)      ((unsigned int)((dev) & MINORMASK))
#define MKDEV(ma, mi)   (((dev_t)(ma) << MINORBITS) | (dev_t)(mi))

#endif /* _LINUX_KDEV_T_H */

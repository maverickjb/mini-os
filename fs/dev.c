/*
 * Character device nodes under /dev — register majors and mknod.
 */

#include <linux/dev.h>
#include <linux/devnull.h>
#include <linux/devtty.h>
#include <linux/devconsole.h>
#include <linux/chrdev.h>
#include <linux/kdev_t.h>
#include <linux/ramfs.h>
#include <linux/errno.h>

static int mem_chrdev_open(dev_t rdev, int flags, struct file **out)
{
    switch (MINOR(rdev)) {
    case 3: /* /dev/null */
        *out = devnull_open(flags);
        return *out ? 0 : -ENOMEM;
    default:
        return -ENODEV;
    }
}

static int ttyaux_chrdev_open(dev_t rdev, int flags, struct file **out)
{
    switch (MINOR(rdev)) {
    case 0: /* /dev/tty */
        *out = devtty_open(flags);
        return *out ? 0 : -ENXIO;
    case 1: /* /dev/console */
        *out = devconsole_open(flags);
        return *out ? 0 : -ENOMEM;
    default:
        return -ENODEV;
    }
}

void dev_init(void)
{
    int err;

    err = ramfs_mkdir("/dev");
    if (err && err != -EEXIST)
        return;

    register_chrdev(MEM_MAJOR, "mem", mem_chrdev_open);
    register_chrdev(TTYAUX_MAJOR, "tty", ttyaux_chrdev_open);

    ramfs_mknod("/dev/null", S_IFCHR | 0666, MKDEV(MEM_MAJOR, 3));
    ramfs_mknod("/dev/tty", S_IFCHR | 0666, MKDEV(TTYAUX_MAJOR, 0));
    ramfs_mknod("/dev/console", S_IFCHR | 0600, MKDEV(TTYAUX_MAJOR, 1));
}

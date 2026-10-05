/*
 * Character device registry — major → open() dispatch (no sysfs/cdev).
 */

#include <linux/chrdev.h>
#include <linux/kdev_t.h>
#include <linux/errno.h>
#include <linux/stddef.h>

#define CHRDEV_MAJOR_MAX 256

struct chrdev_entry {
    const char *name;
    chrdev_open_t open;
};

static struct chrdev_entry chrdevs[CHRDEV_MAJOR_MAX];

int register_chrdev(unsigned int major, const char *name, chrdev_open_t open)
{
    if (major == 0 || major >= CHRDEV_MAJOR_MAX || !open)
        return -EINVAL;

    if (chrdevs[major].open)
        return -EBUSY;

    chrdevs[major].name = name;
    chrdevs[major].open = open;
    return 0;
}

int chrdev_open(dev_t rdev, int flags, struct file **out)
{
    unsigned int major = MAJOR(rdev);
    chrdev_open_t open;

    if (!out)
        return -EINVAL;

    *out = NULL;

    if (major >= CHRDEV_MAJOR_MAX)
        return -ENODEV;

    open = chrdevs[major].open;
    if (!open)
        return -ENODEV;

    return open(rdev, flags, out);
}

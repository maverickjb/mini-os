/*
 * Special /dev path dispatch.
 */

#include <linux/dev.h>
#include <linux/devnull.h>
#include <linux/devtty.h>
#include <linux/devconsole.h>
#include <linux/string.h>

int dev_is_path(const char *path)
{
    return devnull_is_path(path) || devtty_is_path(path) ||
           devconsole_is_path(path);
}

struct file *dev_open_path(const char *path, int flags)
{
    if (!path)
        return NULL;

    if (strcmp(path, "/dev/null") == 0)
        return devnull_open(flags);

    if (strcmp(path, "/dev/tty") == 0)
        return devtty_open(flags);

    if (strcmp(path, "/dev/console") == 0)
        return devconsole_open(flags);

    return NULL;
}

int dev_fill_stat_path(const char *path, struct stat *st)
{
    if (!path || !st)
        return -1;

    if (devnull_is_path(path)) {
        devnull_fill_stat(st);
        return 0;
    }

    if (devtty_is_path(path)) {
        devtty_fill_stat(st);
        return 0;
    }

    if (devconsole_is_path(path)) {
        devconsole_fill_stat(st);
        return 0;
    }

    return -1;
}

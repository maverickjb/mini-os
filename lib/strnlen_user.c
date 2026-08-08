/*
 * Copy a NUL-terminated string from user space (Linux semantics).
 *
 * Returns:
 *   length of the string (excluding NUL) on success,
 *   count if the string is longer than count-1 (no NUL written),
 *   -EFAULT on fault.
 */

#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/stddef.h>

long strncpy_from_user(char *dest, const char __user *src, long count)
{
    long i;

    if (count <= 0)
        return 0;

    if (!dest || !src)
        return -EFAULT;

    for (i = 0; i < count; i++) {
        char c;

        if (copy_from_user(&c, src + i, 1))
            return -EFAULT;

        dest[i] = c;
        if (c == '\0')
            return i;
    }

    return count;
}

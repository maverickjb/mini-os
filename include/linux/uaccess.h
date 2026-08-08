#ifndef __LINUX_UACCESS_H
#define __LINUX_UACCESS_H

#ifndef __user
#define __user
#endif

long strncpy_from_user(char *dest, const char __user *src, long count);

unsigned long copy_from_user(void *dst,
                             const void *src,
                             unsigned long len);

unsigned long copy_to_user(void *dst,
                           const void *src,
                           unsigned long len);

#endif /* __LINUX_UACCESS_H */
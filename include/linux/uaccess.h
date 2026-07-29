#ifndef __LINUX_UACCESS_H
#define __LINUX_UACCESS_H

unsigned long copy_from_user(void *dst,
                             const void *src,
                             unsigned long len);

unsigned long copy_to_user(void *dst,
                           const void *src,
                           unsigned long len);

#endif /* __LINUX_UACCESS_H */
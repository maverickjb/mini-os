#ifndef _LINUX_STDDEF_H
#define _LINUX_STDDEF_H

#define NULL ((void*)0)

#define offsetof(type, member) ((unsigned long)&((type *)0)->member)

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* _LINUX_STDDEF_H */

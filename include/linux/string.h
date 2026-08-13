#ifndef _LINUX_STRING_H
#define _LINUX_STRING_H

#include <linux/stddef.h>

typedef unsigned long size_t;
typedef long ssize_t;

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

size_t strlen(const char *s);
int strcmp(const char *cs, const char *ct);
ssize_t strscpy(char *dest, const char *src, size_t count);

#endif /* _LINUX_STRING_H */

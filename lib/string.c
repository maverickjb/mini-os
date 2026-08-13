#include <linux/string.h>
#include <linux/errno.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;

    return dst;
}

size_t strlen(const char *s)
{
    const char *sc;

    for (sc = s; *sc != '\0'; ++sc)
        ;
    return sc - s;
}

int strcmp(const char *cs, const char *ct)
{
    unsigned char c1;
    unsigned char c2;

    while (1) {
        c1 = *cs++;
        c2 = *ct++;

        if (c1 != c2)
            return c1 < c2 ? -1 : 1;

        if (!c1)
            break;
    }

    return 0;
}

ssize_t strscpy(char *dest, const char *src, size_t count)
{
    size_t i;

    if (count == 0)
        return -E2BIG;

    for (i = 0; i < count - 1; i++) {
        dest[i] = src[i];
        if (src[i] == '\0')
            return (ssize_t)i;
    }

    dest[count - 1] = '\0';
    return -E2BIG;
}

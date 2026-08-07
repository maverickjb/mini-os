/*
 * Minimal stdio — stdout via write(2).
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int putchar(int c)
{
    char ch = (char)c;

    if (write(STDOUT_FILENO, &ch, 1) != 1)
        return -1;
    return (unsigned char)ch;
}

int puts(const char *s)
{
    size_t n;

    if (!s)
        return -1;

    n = strlen(s);
    if (n && write(STDOUT_FILENO, s, n) < 0)
        return -1;
    if (putchar('\n') < 0)
        return -1;
    return (int)n + 1;
}

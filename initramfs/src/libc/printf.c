/*
 * Minimal printf — supports %s %c %d %u %x %p %% and literal text.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

static void out_char(char **bufp, char *end, char ch, int *count)
{
    (*count)++;
    if (!bufp) {
        putchar(ch);
        return;
    }
    if (*bufp < end)
        *(*bufp)++ = ch;
}

static void out_str(char **bufp, char *end, const char *s, int *count)
{
    if (!s)
        s = "(null)";
    while (*s)
        out_char(bufp, end, *s++, count);
}

static void out_uint(char **bufp, char *end, unsigned long v, unsigned base,
                     int *count, int upper)
{
    char tmp[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (v == 0) {
        out_char(bufp, end, '0', count);
        return;
    }

    while (v > 0) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    while (i--)
        out_char(bufp, end, tmp[i], count);
}

static int vformat(char *buf, size_t bufsz, const char *fmt, va_list ap)
{
    char *bp = buf;
    char *end = buf ? buf + (bufsz ? bufsz - 1 : 0) : NULL;
    char **bufp = buf ? &bp : NULL;
    int count = 0;

    if (!fmt)
        return -1;

    while (*fmt) {
        if (*fmt != '%') {
            out_char(bufp, end, *fmt++, &count);
            continue;
        }
        fmt++;
        switch (*fmt) {
        case '%':
            out_char(bufp, end, '%', &count);
            break;
        case 'c':
            out_char(bufp, end, (char)va_arg(ap, int), &count);
            break;
        case 's':
            out_str(bufp, end, va_arg(ap, const char *), &count);
            break;
        case 'd': {
            long v = va_arg(ap, int);

            if (v < 0) {
                out_char(bufp, end, '-', &count);
                out_uint(bufp, end, (unsigned long)(-v), 10, &count, 0);
            } else {
                out_uint(bufp, end, (unsigned long)v, 10, &count, 0);
            }
            break;
        }
        case 'u':
            out_uint(bufp, end, (unsigned long)va_arg(ap, unsigned), 10,
                     &count, 0);
            break;
        case 'x':
            out_uint(bufp, end, (unsigned long)va_arg(ap, unsigned), 16,
                     &count, 0);
            break;
        case 'p': {
            unsigned long v = (unsigned long)va_arg(ap, void *);

            out_str(bufp, end, "0x", &count);
            out_uint(bufp, end, v, 16, &count, 0);
            break;
        }
        case '\0':
            return count;
        default:
            out_char(bufp, end, '%', &count);
            out_char(bufp, end, *fmt, &count);
            break;
        }
        fmt++;
    }

    if (buf && bufsz)
        *bp = '\0';
    return count;
}

int vprintf(const char *fmt, va_list ap)
{
    return vformat(NULL, 0, fmt, ap);
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    /* No bound — caller must provide a large enough buffer. */
    return vformat(buf, 0x7fffffffUL, fmt, ap);
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

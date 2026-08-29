/*
 * Minimal kernel vsnprintf — enough for early printk.
 */

#include <stdarg.h>
#include <linux/string.h>
#include <linux/types.h>

static char *emit_char(char *p, char *end, char c)
{
    if (p < end)
        *p = c;
    return p + 1;
}

static char *emit_str(char *p, char *end, const char *s)
{
    if (!s)
        s = "(null)";

    while (*s)
        p = emit_char(p, end, *s++);
    return p;
}

static char *emit_ulong(char *p, char *end, unsigned long v, int base,
                        int upper)
{
    char tmp[2 * sizeof(unsigned long)];
    int i = 0;
    static const char lower[] = "0123456789abcdef";
    static const char up[] = "0123456789ABCDEF";
    const char *digits = upper ? up : lower;

    if (v == 0)
        return emit_char(p, end, '0');

    while (v) {
        tmp[i++] = digits[v % (unsigned long)base];
        v /= (unsigned long)base;
    }

    while (i > 0)
        p = emit_char(p, end, tmp[--i]);
    return p;
}

static char *emit_long(char *p, char *end, long v)
{
    if (v < 0) {
        p = emit_char(p, end, '-');
        v = -(unsigned long)v;
    }
    return emit_ulong(p, end, (unsigned long)v, 10, 0);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    char *p = buf;
    char *end = buf + size;

    if (!fmt) {
        if (size > 0)
            buf[0] = '\0';
        return 0;
    }

    if (size == 0)
        end = buf;

    while (*fmt) {
        if (*fmt != '%') {
            p = emit_char(p, end, *fmt++);
            continue;
        }

        fmt++;
        if (*fmt == '%') {
            p = emit_char(p, end, '%');
            fmt++;
            continue;
        }

        switch (*fmt) {
        case 's':
            p = emit_str(p, end, va_arg(ap, const char *));
            fmt++;
            break;
        case 'c': {
            int c = va_arg(ap, int);

            p = emit_char(p, end, (char)c);
            fmt++;
            break;
        }
        case 'd':
        case 'i':
            p = emit_long(p, end, va_arg(ap, int));
            fmt++;
            break;
        case 'u':
            p = emit_ulong(p, end, va_arg(ap, unsigned int), 10, 0);
            fmt++;
            break;
        case 'x':
            p = emit_ulong(p, end, va_arg(ap, unsigned int), 16, 0);
            fmt++;
            break;
        case 'l':
            fmt++;
            if (*fmt == 'x') {
                p = emit_ulong(p, end, va_arg(ap, unsigned long), 16, 0);
                fmt++;
            } else if (*fmt == 'u') {
                p = emit_ulong(p, end, va_arg(ap, unsigned long), 10, 0);
                fmt++;
            } else if (*fmt == 'd') {
                p = emit_long(p, end, va_arg(ap, long));
                fmt++;
            } else {
                p = emit_char(p, end, '%');
                p = emit_char(p, end, 'l');
                if (*fmt)
                    p = emit_char(p, end, *fmt++);
            }
            break;
        case 'p': {
            void *ptr = va_arg(ap, void *);

            p = emit_char(p, end, '0');
            p = emit_char(p, end, 'x');
            if (!ptr)
                p = emit_str(p, end, "0");
            else
                p = emit_ulong(p, end, (unsigned long)ptr, 16, 0);
            fmt++;
            break;
        }
        default:
            p = emit_char(p, end, '%');
            if (*fmt)
                p = emit_char(p, end, *fmt++);
            break;
        }
    }

    if (size > 0) {
        if ((size_t)(p - buf) >= size)
            buf[size - 1] = '\0';
        else
            *p = '\0';
    }

    return (int)(p - buf);
}

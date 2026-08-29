/*
 * printk — formatted console output via UART.
 */

#include <stdarg.h>
#include <linux/printk.h>
#include <linux/serial.h>
#include <linux/string.h>

void printk(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    if (!fmt)
        return;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    uart_write(buf);
}

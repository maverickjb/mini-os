/*
 * PL011 UART console driver for QEMU virt (MMIO 0x09000000).
 */

#include <linux/fs.h>
#include <linux/serial.h>
#include <linux/errno.h>
#include "mem.h"

#define UART0_VIRT      ((unsigned long)__phys_to_virt(0x09000000UL))

#define UART_DR         (*(volatile unsigned int *)(UART0_VIRT + 0x00))
#define UART_FR         (*(volatile unsigned int *)(UART0_VIRT + 0x18))
#define UART_FR_TXFF    (1u << 5)

void serial_putc(char c)
{
    if (c == '\n')
        serial_putc('\r');

    while (UART_FR & UART_FR_TXFF)
        ;
    UART_DR = (unsigned int)c;
}

void uart_putc(char c)
{
    serial_putc(c);
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static long serial_write(struct file *file, const char *buf,
                         unsigned long count, long *pos)
{
    unsigned long i;

    (void)file;
    (void)pos;

    if (!buf)
        return -EFAULT;

    for (i = 0; i < count; i++)
        serial_putc(buf[i]);

    return (long)count;
}

static const struct file_operations serial_fops = {
    .write = serial_write,
};

struct file uart_file = {
    .f_op = &serial_fops,
    .private_data = 0,
    .f_pos = 0,
};

void serial_init(void)
{
}

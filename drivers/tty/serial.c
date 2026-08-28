/*
 * PL011 UART console driver for QEMU virt (MMIO 0x09000000).
 */

#include <linux/fs.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <asm/memory.h>

#define UART0_VIRT      ((unsigned long)__phys_to_virt(0x09000000UL))

#define UART_DR         (*(volatile unsigned int *)(UART0_VIRT + 0x00))
#define UART_FR         (*(volatile unsigned int *)(UART0_VIRT + 0x18))
#define UART_LCRH       (*(volatile unsigned int *)(UART0_VIRT + 0x2c))
#define UART_CR         (*(volatile unsigned int *)(UART0_VIRT + 0x30))
#define UART_IMSC       (*(volatile unsigned int *)(UART0_VIRT + 0x38))
#define UART_ICR        (*(volatile unsigned int *)(UART0_VIRT + 0x44))
#define UART_FR_RXFE    (1u << 4)
#define UART_FR_TXFF    (1u << 5)
#define UART_CR_UARTEN  (1u << 0)
#define UART_CR_TXE     (1u << 8)
#define UART_CR_RXE     (1u << 9)
#define UART_LCRH_WLEN8 (3u << 5)
#define UART_IMSC_RXIM  (1u << 4)
#define UART_IMSC_RTIM  (1u << 6)

void serial_putc(char c)
{
    if (c == '\n')
        serial_putc('\r');

    while (UART_FR & UART_FR_TXFF)
        ;
    UART_DR = (unsigned int)c;
}

int serial_rx_ready(void)
{
    return !(UART_FR & UART_FR_RXFE);
}

char serial_getc(void)
{
    return (char)(UART_DR & 0xff);
}

void serial_irq(void)
{
    while (serial_rx_ready())
        tty_receive_char(serial_getc());
    UART_ICR = 0x7ff;
}

void serial_rx_enable(void)
{
    while (serial_rx_ready())
        (void)serial_getc();
    UART_ICR = 0x7ff;
    /* RXIM plus receive-timeout: a 1-byte FIFO fill does not raise RXIM. */
    UART_IMSC = UART_IMSC_RXIM | UART_IMSC_RTIM;
}

void serial_init(void)
{
    UART_CR = 0;
    UART_ICR = 0x7ff;
    /* 8N1, FIFOs off so each byte raises an RX interrupt. */
    UART_LCRH = UART_LCRH_WLEN8;
    UART_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
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

static struct file_ops serial_fops = {
    .write = serial_write,
};

struct file uart_file = {
    .refcount = 0,
    .inode = NULL,
    .f_op = &serial_fops,
    .private_data = NULL,
    .f_pos = 0,
    .f_flags = 0,
    .f_mode = 0,
};


/*
 * PL011 UART console driver for QEMU virt (MMIO 0x09000000).
 *
 * All MMIO access is serialized with uart_lock so printk / tty_write /
 * RX IRQ handlers on different CPUs cannot corrupt the UART or wedge TX.
 */

#include <linux/fs.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/spinlock.h>
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

static spinlock_t uart_lock = SPINLOCK_INIT;

static void serial_putc_unlocked(char c)
{
    if (c == '\n')
        serial_putc_unlocked('\r');

    while (UART_FR & UART_FR_TXFF)
        ;
    UART_DR = (unsigned int)c;
}

static int serial_rx_ready_unlocked(void)
{
    return !(UART_FR & UART_FR_RXFE);
}

static char serial_getc_unlocked(void)
{
    return (char)(UART_DR & 0xff);
}

void serial_putc(char c)
{
    unsigned long flags;

    spin_lock_irqsave(&uart_lock, flags);
    serial_putc_unlocked(c);
    spin_unlock_irqrestore(&uart_lock, flags);
}

int serial_rx_ready(void)
{
    unsigned long flags;
    int ready;

    spin_lock_irqsave(&uart_lock, flags);
    ready = serial_rx_ready_unlocked();
    spin_unlock_irqrestore(&uart_lock, flags);
    return ready;
}

char serial_getc(void)
{
    unsigned long flags;
    char c;

    spin_lock_irqsave(&uart_lock, flags);
    c = serial_getc_unlocked();
    spin_unlock_irqrestore(&uart_lock, flags);
    return c;
}

void serial_irq(void)
{
    unsigned long flags;
    char c;

    spin_lock_irqsave(&uart_lock, flags);
    while (serial_rx_ready_unlocked()) {
        c = serial_getc_unlocked();
        /*
         * Drop uart_lock before tty_receive_char(): echo may call
         * serial_putc() and must not deadlock on this lock.
         */
        spin_unlock_irqrestore(&uart_lock, flags);
        tty_receive_char(c);
        spin_lock_irqsave(&uart_lock, flags);
    }
    UART_ICR = 0x7ff;
    spin_unlock_irqrestore(&uart_lock, flags);
}

void serial_rx_enable(void)
{
    unsigned long flags;

    spin_lock_irqsave(&uart_lock, flags);
    while (serial_rx_ready_unlocked())
        (void)serial_getc_unlocked();
    UART_ICR = 0x7ff;
    /* RXIM plus receive-timeout: a 1-byte FIFO fill does not raise RXIM. */
    UART_IMSC = UART_IMSC_RXIM | UART_IMSC_RTIM;
    spin_unlock_irqrestore(&uart_lock, flags);
}

void serial_init(void)
{
    unsigned long flags;

    spin_lock_irqsave(&uart_lock, flags);
    UART_CR = 0;
    UART_ICR = 0x7ff;
    /* 8N1, FIFOs off so each byte raises an RX interrupt. */
    UART_LCRH = UART_LCRH_WLEN8;
    UART_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    spin_unlock_irqrestore(&uart_lock, flags);
}

void uart_putc(char c)
{
    serial_putc(c);
}

void uart_puts(const char *s)
{
    uart_write(s);
}

void uart_write(const char *s)
{
    unsigned long flags;

    if (!s)
        return;

    /* Hold the lock across the string so SMP printk lines stay intact. */
    spin_lock_irqsave(&uart_lock, flags);
    while (*s)
        serial_putc_unlocked(*s++);
    spin_unlock_irqrestore(&uart_lock, flags);
}

void serial_write_n(const char *buf, unsigned long count)
{
    unsigned long i;
    unsigned long flags;

    if (!buf || !count)
        return;

    spin_lock_irqsave(&uart_lock, flags);
    for (i = 0; i < count; i++)
        serial_putc_unlocked(buf[i]);
    spin_unlock_irqrestore(&uart_lock, flags);
}

static long serial_write(struct file *file, const char *buf,
                         unsigned long count, long *pos)
{
    (void)file;
    (void)pos;

    if (!buf)
        return -EFAULT;

    serial_write_n(buf, count);
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

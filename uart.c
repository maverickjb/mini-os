/*
 * PL011 UART driver for QEMU virt (base 0x09000000)
 */

#include "uart.h"

#define UART0_BASE  0x09000000UL

#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x18))

/* FR bit 5: TX FIFO full */
#define UART_FR_TXFF  (1u << 5)

void uart_putc(char c)
{
    if (c == '\n')
        uart_putc('\r');

    while (UART_FR & UART_FR_TXFF)
        ;
    UART_DR = (unsigned int)c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

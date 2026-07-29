#ifndef __LINUX_SERIAL_H
#define __LINUX_SERIAL_H

#include <linux/fs.h>

void serial_init(void);
void serial_putc(char c);
void uart_putc(char c);
void uart_puts(const char *s);

extern struct file uart_file;

#endif

#ifndef _LINUX_TTY_H
#define _LINUX_TTY_H

#include <linux/types.h>

struct serial_device;
struct task_struct;

#define TTY_RX_SIZE	256

#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410

struct tty {
    struct serial_device *serial;

    char rx_buf[TTY_RX_SIZE];
    unsigned int rx_head;
    unsigned int rx_tail;

    pid_t session_id;
    pid_t foreground_pgid;

    int echo;
    int canonical;

    struct task_struct *read_wait;
};

extern struct tty tty0;

void tty_init(void);
void tty_attach_session(pid_t sid, pid_t pgid);
void tty_receive_char(char c);

long tty_read(char *buf, unsigned long count);
long tty_write(const char *buf, unsigned long count);

pid_t tty_getpgrp(void);
int tty_setpgrp(pid_t pgid);

#endif /* _LINUX_TTY_H */

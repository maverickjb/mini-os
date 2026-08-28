#ifndef _LINUX_TTY_H
#define _LINUX_TTY_H

#include <linux/types.h>

struct file;
struct serial_device;
struct task_struct;

#define TTY_RX_SIZE	256

#define TCGETS		0x5401
#define TCSETS		0x5402
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCSCTTY	0x540E
#define TIOCGSID	0x5429
#define TIOCNOTTY	0x5422
#define TIOCGWINSZ	0x5413

#define NCCS		32
#define VINTR		0
#define VQUIT		1
#define VERASE		2
#define VKILL		3
#define VEOF		4
#define VTIME		5
#define VMIN		6

/* termios c_iflag */
#define IGNBRK		0x0001
#define ICRNL		0x0400

/* termios c_oflag */
#define OPOST		0x0001
#define ONLCR		0x0004

/* termios c_cflag */
#define CS8		0x0030
#define CREAD		0x0800
#define HUPCL		0x0400

/* termios c_lflag */
#define ISIG		0x0001
#define ICANON		0x0002
#define ECHO		0x0008
#define ECHOE		0x0010
#define ECHOK		0x0020
#define IEXTEN		0x8000

/*
 * Linux/musl aarch64 struct termios layout (60 bytes).
 */
struct user_termios {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_line;
    unsigned char __pad[3];
    unsigned char c_cc[NCCS];
    unsigned int ispeed;
    unsigned int ospeed;
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct tty {
    struct serial_device *serial;

    char rx_buf[TTY_RX_SIZE];
    unsigned int rx_head;
    unsigned int rx_tail;

    pid_t session_id;
    pid_t foreground_pgid;

    int echo;
    int canonical;
    int isig;

    struct user_termios termios;

    struct task_struct *read_wait;
};

extern struct tty tty0;
extern struct file_ops tty_fops;

void tty_init(void);
void tty_attach_session(pid_t sid, pid_t pgid);
void tty_receive_char(char c);

long tty_read(char *buf, unsigned long count);
long tty_write(const char *buf, unsigned long count);

pid_t tty_getpgrp(void);
int tty_setpgrp(pid_t pgid);
int tty_sets_controlling(struct file *file, int force);
int tty_release_controlling(void);

#endif /* _LINUX_TTY_H */

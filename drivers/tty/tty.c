/*
 * Console TTY — UART-backed, with a small RX ring and foreground pgrp.
 *
 * tty0.lock protects the RX ring and termios-derived flags against
 * concurrent readers and serial_irq() on another CPU.
 */

#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <asm/irqflags.h>

struct tty tty0;

static unsigned int tty_rx_next(unsigned int i)
{
    i++;
    if (i == TTY_RX_SIZE)
        i = 0;
    return i;
}

/* Caller must hold tty0.lock. */
static unsigned int tty_rx_count_locked(void)
{
    unsigned int n;

    if (tty0.rx_head >= tty0.rx_tail)
        n = tty0.rx_head - tty0.rx_tail;
    else
        n = TTY_RX_SIZE - tty0.rx_tail + tty0.rx_head;
    return n;
}

/* Caller must hold tty0.lock. */
static unsigned int tty_rx_line_length_locked(void)
{
    unsigned int n = 0;
    unsigned int i;

    i = tty0.rx_tail;
    while (i != tty0.rx_head) {
        n++;
        if (tty0.rx_buf[i] == '\n')
            return n;
        i = tty_rx_next(i);
    }
    return 0;
}

/* Caller must hold tty0.lock. */
static int tty_read_ready_locked(void)
{
    if (tty0.canonical)
        return tty_rx_line_length_locked() > 0;

    return tty_rx_count_locked() > 0;
}

static int tty_read_ready(void)
{
    unsigned long flags;
    int ready;

    spin_lock_irqsave(&tty0.lock, flags);
    ready = tty_read_ready_locked();
    spin_unlock_irqrestore(&tty0.lock, flags);
    return ready;
}

static void tty_wake_reader(void);

static void tty_apply_termios(const struct user_termios *t)
{
    unsigned long flags;

    spin_lock_irqsave(&tty0.lock, flags);
    tty0.termios = *t;
    tty0.canonical = !!(t->c_lflag & ICANON);
    tty0.echo = !!(t->c_lflag & ECHO);
    tty0.isig = !!(t->c_lflag & ISIG);
    spin_unlock_irqrestore(&tty0.lock, flags);
    tty_wake_reader();
}

static void tty_default_termios(struct user_termios *t)
{
    unsigned int i;

    for (i = 0; i < sizeof(*t); i++)
        ((unsigned char *)t)[i] = 0;

    t->c_iflag = ICRNL;
    t->c_oflag = OPOST | ONLCR;
    t->c_cflag = CS8 | CREAD | HUPCL;
    t->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN;
    t->c_cc[VEOF] = 4;   /* ^D */
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}

static void tty_wake_reader(void)
{
    wake_up(&tty0.read_wait);
}

void tty_receive_char(char c)
{
    unsigned int next;
    int do_echo = 0;
    unsigned long flags;

    if (c == '\r')
        c = '\n';

    spin_lock_irqsave(&tty0.lock, flags);

    if (c == 0x03) {
        if (tty0.isig && tty0.foreground_pgid) {
            pid_t pgid = tty0.foreground_pgid;

            spin_unlock_irqrestore(&tty0.lock, flags);
            ksys_kill(-(long)pgid, SIGINT);
            return;
        }
        spin_unlock_irqrestore(&tty0.lock, flags);
        return;
    }

    if (c == 0x1a) {
        if (tty0.isig && tty0.foreground_pgid) {
            pid_t pgid = tty0.foreground_pgid;

            spin_unlock_irqrestore(&tty0.lock, flags);
            ksys_kill(-(long)pgid, SIGTSTP);
            return;
        }
        spin_unlock_irqrestore(&tty0.lock, flags);
        return;
    }

    next = tty_rx_next(tty0.rx_head);
    if (next != tty0.rx_tail) {
        tty0.rx_buf[tty0.rx_head] = c;
        tty0.rx_head = next;
    }

    do_echo = tty0.echo;
    spin_unlock_irqrestore(&tty0.lock, flags);

    /* Echo outside tty lock — serial_putc takes uart_lock. */
    if (do_echo)
        serial_putc(c);

    tty_wake_reader();
}

long tty_read(char *buf, unsigned long count)
{
    unsigned long n;
    unsigned long i;
    unsigned long flags;

    if (!buf)
        return -EFAULT;

    if (count == 0)
        return 0;

    if (wait_event_interruptible(&tty0.read_wait, tty_read_ready))
        return -EINTR;

    spin_lock_irqsave(&tty0.lock, flags);

    if (!tty_read_ready_locked()) {
        spin_unlock_irqrestore(&tty0.lock, flags);
        return 0;
    }

    if (tty0.canonical)
        n = tty_rx_line_length_locked();
    else
        n = tty_rx_count_locked();

    if (n > count)
        n = count;

    for (i = 0; i < n; i++) {
        buf[i] = tty0.rx_buf[tty0.rx_tail];
        tty0.rx_tail = tty_rx_next(tty0.rx_tail);
    }

    spin_unlock_irqrestore(&tty0.lock, flags);

    return (long)n;
}

long tty_write(const char *buf, unsigned long count)
{
    if (!buf)
        return -EFAULT;

    /* One lock for the whole write so concurrent CPUs don't interleave. */
    serial_write_n(buf, count);
    return (long)count;
}

pid_t tty_getpgrp(void)
{
    unsigned long flags;
    pid_t pgid;

    spin_lock_irqsave(&tty0.lock, flags);
    pgid = tty0.foreground_pgid;
    spin_unlock_irqrestore(&tty0.lock, flags);
    return pgid;
}

int tty_setpgrp(pid_t pgid)
{
    struct list_head *pos;
    struct task_struct *task;
    int found = 0;
    unsigned long flags;
    unsigned long tty_flags;
    pid_t session;

    if (pgid <= 0)
        return -EINVAL;

    if (!current)
        return -ENOTTY;

    spin_lock_irqsave(&tty0.lock, tty_flags);
    session = tty0.session_id;
    if (session && current->sid != session) {
        spin_unlock_irqrestore(&tty0.lock, tty_flags);
        return -ENOTTY;
    }
    spin_unlock_irqrestore(&tty0.lock, tty_flags);

    task_list_lock_irqsave(&flags);
    for_each_task(pos, task) {
        if (!task->is_user ||
            task->state == TASK_ZOMBIE || task->state == TASK_DEAD)
            continue;
        if (task->pgid != pgid)
            continue;
        found = 1;
        if (session && task->sid != session) {
            task_list_unlock_irqrestore(flags);
            return -EPERM;
        }
    }
    task_list_unlock_irqrestore(flags);

    if (!found)
        return -ESRCH;

    spin_lock_irqsave(&tty0.lock, tty_flags);
    tty0.foreground_pgid = pgid;
    spin_unlock_irqrestore(&tty0.lock, tty_flags);
    return 0;
}

int tty_sets_controlling(struct file *file, int force)
{
    struct task_struct *task = current;
    unsigned long flags;

    (void)file;
    (void)force;

    if (!task || !task->is_user)
        return -ENOTTY;

    if (task->sid != task->pid)
        return -ENOTTY;

    spin_lock_irqsave(&tty0.lock, flags);
    tty0.session_id = task->sid;
    tty0.foreground_pgid = task->pgid;
    spin_unlock_irqrestore(&tty0.lock, flags);
    return 0;
}

int tty_release_controlling(void)
{
    struct task_struct *task = current;
    unsigned long flags;

    if (!task || !task->is_user)
        return -ENOTTY;

    spin_lock_irqsave(&tty0.lock, flags);
    if (tty0.session_id != task->sid) {
        spin_unlock_irqrestore(&tty0.lock, flags);
        return -ENOTTY;
    }

    tty0.session_id = 0;
    tty0.foreground_pgid = 0;
    spin_unlock_irqrestore(&tty0.lock, flags);
    return 0;
}

static long tty_file_read(struct file *file, char *buf, unsigned long count,
                          long *pos)
{
    (void)file;
    (void)pos;
    return tty_read(buf, count);
}

static long tty_file_write(struct file *file, const char *buf,
                           unsigned long count, long *pos)
{
    (void)file;
    (void)pos;
    return tty_write(buf, count);
}

static long tty_file_ioctl(struct file *file, unsigned int cmd,
                           unsigned long arg)
{
    pid_t pgid;
    unsigned long flags;

    (void)file;

    switch (cmd) {
    case TCGETS: {
        struct user_termios t;

        if (!arg)
            return -EFAULT;
        spin_lock_irqsave(&tty0.lock, flags);
        t = tty0.termios;
        spin_unlock_irqrestore(&tty0.lock, flags);
        if (copy_to_user((void *)arg, &t, sizeof(t)))
            return -EFAULT;
        return 0;
    }
    case TCSETS: {
        struct user_termios t;

        if (!arg || copy_from_user(&t, (void *)arg, sizeof(t)))
            return -EFAULT;
        tty_apply_termios(&t);
        return 0;
    }
    case TIOCGPGRP:
        pgid = tty_getpgrp();
        if (copy_to_user((pid_t *)arg, &pgid, sizeof(pgid)))
            return -EFAULT;
        return 0;
    case TIOCSPGRP:
        if (copy_from_user(&pgid, (pid_t *)arg, sizeof(pgid)))
            return -EFAULT;
        return tty_setpgrp(pgid);
    case TIOCSCTTY:
        return tty_sets_controlling(file, (int)arg);
    case TIOCGSID:
        spin_lock_irqsave(&tty0.lock, flags);
        pgid = tty0.session_id;
        spin_unlock_irqrestore(&tty0.lock, flags);
        if (copy_to_user((pid_t *)arg, &pgid, sizeof(pgid)))
            return -EFAULT;
        return 0;
    case TIOCNOTTY:
        return tty_release_controlling();
    case TIOCGWINSZ: {
        struct winsize ws = {
            .ws_row = 24,
            .ws_col = 80,
            .ws_xpixel = 0,
            .ws_ypixel = 0,
        };

        if (!arg || copy_to_user((void *)arg, &ws, sizeof(ws)))
            return -EFAULT;
        return 0;
    }
    default:
        return -ENOTTY;
    }
}

struct file_ops tty_fops = {
    .read = tty_file_read,
    .write = tty_file_write,
    .ioctl = tty_file_ioctl,
};

void tty_attach_session(pid_t sid, pid_t pgid)
{
    unsigned long flags;

    spin_lock_irqsave(&tty0.lock, flags);
    tty0.session_id = sid;
    tty0.foreground_pgid = pgid;
    spin_unlock_irqrestore(&tty0.lock, flags);
}

void tty_init(void)
{
    tty0.serial = NULL;
    tty0.rx_head = 0;
    tty0.rx_tail = 0;
    tty0.session_id = 0;
    tty0.foreground_pgid = 0;
    spin_lock_init(&tty0.lock);
    init_waitqueue_head(&tty0.read_wait);

    tty_default_termios(&tty0.termios);
    tty_apply_termios(&tty0.termios);

    uart_file.f_op = &tty_fops;

    irq_enable(IRQ_UART);
    serial_rx_enable();
}

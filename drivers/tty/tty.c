/*
 * Console TTY — UART-backed, with a small RX ring and foreground pgrp.
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
#include <asm/irqflags.h>

struct tty tty0;

static unsigned int tty_rx_next(unsigned int i)
{
    i++;
    if (i == TTY_RX_SIZE)
        i = 0;
    return i;
}

static unsigned int tty_rx_count(void)
{
    unsigned int n;

    if (tty0.rx_head >= tty0.rx_tail)
        n = tty0.rx_head - tty0.rx_tail;
    else
        n = TTY_RX_SIZE - tty0.rx_tail + tty0.rx_head;
    return n;
}

static unsigned int tty_rx_line_length(void)
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

static int input_not_ready(void)
{
    if (tty_rx_count() == 0)
        return 1;
    if (tty0.canonical && tty_rx_line_length() == 0)
        return 1;
    return 0;
}

static void tty_wake_reader(void)
{
    if (tty0.read_wait)
        wake_up_process(tty0.read_wait);
}

void tty_receive_char(char c)
{
    unsigned int next;

    if (c == '\r')
        c = '\n';

    if (c == 0x03) {
        if (tty0.foreground_pgid)
            ksys_kill(-(long)tty0.foreground_pgid, SIGINT);
        return;
    }

    if (c == 0x1a) {
        if (tty0.foreground_pgid)
            ksys_kill(-(long)tty0.foreground_pgid, SIGTSTP);
        return;
    }

    next = tty_rx_next(tty0.rx_head);
    if (next != tty0.rx_tail) {
        tty0.rx_buf[tty0.rx_head] = c;
        tty0.rx_head = next;
    }

    if (tty0.echo)
        serial_putc(c);

    tty_wake_reader();
}

long tty_read(char *buf, unsigned long count)
{
    unsigned long n;
    unsigned long i;

    if (!buf)
        return -EFAULT;
    if (!count)
        return 0;

retry:
    local_irq_disable();

    if (input_not_ready()) {
        if (signal_pending(current)) {
            local_irq_enable();
            return -EINTR;
        }

        /* Atomically prepare to sleep. */
        tty0.read_wait = current;
        current->state = TASK_SLEEPING;

        local_irq_enable();
        schedule();

        local_irq_disable();

        tty0.read_wait = NULL;
        current->state = TASK_RUNNING;
        current->time_slice = SCHED_TIME_SLICE;

        local_irq_enable();

        if (signal_pending(current))
            return -EINTR;

        goto retry;
    }

    if (tty0.canonical)
        n = tty_rx_line_length();
    else
        n = tty_rx_count();

    if (n > count)
        n = count;

    for (i = 0; i < n; i++) {
        buf[i] = tty0.rx_buf[tty0.rx_tail];
        tty0.rx_tail = tty_rx_next(tty0.rx_tail);
    }

    local_irq_enable();
    return (long)n;
}

long tty_write(const char *buf, unsigned long count)
{
    unsigned long i;

    if (!buf)
        return -EFAULT;

    for (i = 0; i < count; i++)
        serial_putc(buf[i]);

    return (long)count;
}

pid_t tty_getpgrp(void)
{
    return tty0.foreground_pgid;
}

int tty_setpgrp(pid_t pgid)
{
    struct task_struct *task;
    int found = 0;

    if (pgid <= 0)
        return -EINVAL;

    if (!current)
        return -ENOTTY;

    if (tty0.session_id && current->sid != tty0.session_id)
        return -ENOTTY;

    for (task = runqueue; task; task = task->next) {
        if (!task->is_user ||
            task->state == TASK_ZOMBIE || task->state == TASK_DEAD)
            continue;
        if (task->pgid != pgid)
            continue;
        found = 1;
        if (tty0.session_id && task->sid != tty0.session_id)
            return -EPERM;
    }

    if (!found)
        return -ESRCH;

    tty0.foreground_pgid = pgid;
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

    (void)file;

    switch (cmd) {
    case TCGETS: {
        unsigned char termios[60];
        unsigned int i;

        for (i = 0; i < sizeof(termios); i++)
            termios[i] = 0;
        if (!arg || copy_to_user((void *)arg, termios, sizeof(termios)))
            return -EFAULT;
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
    default:
        return -ENOTTY;
    }
}

static struct file_ops tty_fops = {
    .read = tty_file_read,
    .write = tty_file_write,
    .ioctl = tty_file_ioctl,
};

void tty_attach_session(pid_t sid, pid_t pgid)
{
    tty0.session_id = sid;
    tty0.foreground_pgid = pgid;
}

void tty_init(void)
{
    tty0.serial = NULL;
    tty0.rx_head = 0;
    tty0.rx_tail = 0;
    tty0.session_id = 0;
    tty0.foreground_pgid = 0;
    tty0.echo = 1;
    tty0.canonical = 1;
    tty0.read_wait = NULL;

    uart_file.f_op = &tty_fops;

    irq_enable(IRQ_UART);
    serial_rx_enable();
}

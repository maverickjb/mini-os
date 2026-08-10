/*
 * pipe(2) / pipe2(2) — anonymous pipe with a ring buffer and wait queues.
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/stddef.h>
#include <asm/irqflags.h>

#define PIPE_SIZE 1024

struct wait_node {
    struct task_struct *task;
    struct wait_node *next;
};

struct pipe {
    unsigned int head;
    unsigned int tail;
    unsigned int len;
    int readers;
    int writers;
    struct wait_node *read_wait;
    struct wait_node *write_wait;
    char buf[PIPE_SIZE];
};

static void add_wait_queue(struct wait_node **queue, struct task_struct *task)
{
    struct wait_node *node;

    node = alloc_pages(0);
    if (!node)
        return;

    node->task = task;
    node->next = *queue;
    *queue = node;
}

static void remove_wait_queue(struct wait_node **queue, struct task_struct *task)
{
    struct wait_node **prev = queue;
    struct wait_node *node;

    while (*prev) {
        if ((*prev)->task == task) {
            node = *prev;
            *prev = node->next;
            free_pages(node, 0);
            return;
        }
        prev = &(*prev)->next;
    }
}

static void wake_wait_queue(struct wait_node **queue)
{
    struct wait_node *node;

    while (*queue) {
        node = *queue;
        *queue = node->next;
        wake_up_process(node->task);
        free_pages(node, 0);
    }
}

static void pipe_sleep(struct wait_node **queue)
{
    struct task_struct *task = current;

    if (!task)
        return;

    local_irq_disable();
    add_wait_queue(queue, task);
    task->state = TASK_SLEEPING;
    local_irq_enable();

    schedule();

    local_irq_disable();
    remove_wait_queue(queue, task);
    local_irq_enable();

    task->state = TASK_RUNNING;
    task->time_slice = SCHED_TIME_SLICE;
}

static long pipe_read(struct file *file, char *buf, unsigned long count,
                      long *pos)
{
    struct pipe *p = file->private_data;
    unsigned long n;
    unsigned long i;

    (void)pos;

    if (!p || !buf)
        return -EINVAL;

retry:
    local_irq_disable();

    if (p->len == 0) {
        if (p->writers == 0) {
            local_irq_enable();
            return 0;
        }

        local_irq_enable();
        pipe_sleep(&p->read_wait);
        if (current->pending)
            return -EINTR;
        goto retry;
    }

    n = count;
    if (n > p->len)
        n = p->len;

    for (i = 0; i < n; i++) {
        buf[i] = p->buf[p->tail];
        p->tail++;
        if (p->tail == PIPE_SIZE)
            p->tail = 0;
    }

    p->len -= (unsigned int)n;
    local_irq_enable();

    wake_wait_queue(&p->write_wait);
    return (long)n;
}

static long pipe_write(struct file *file, const char *buf, unsigned long count,
                       long *pos)
{
    struct pipe *p = file->private_data;
    unsigned long n;
    unsigned long i;

    (void)pos;

    if (!p || !buf)
        return -EINVAL;

retry:
    local_irq_disable();

    if (p->readers == 0) {
        local_irq_enable();
        return -EPIPE;
    }

    if (p->len == PIPE_SIZE) {
        local_irq_enable();
        pipe_sleep(&p->write_wait);
        if (current->pending)
            return -EINTR;
        goto retry;
    }

    n = count;
    if (n > PIPE_SIZE - p->len)
        n = PIPE_SIZE - p->len;

    for (i = 0; i < n; i++) {
        p->buf[p->head] = buf[i];
        p->head++;
        if (p->head == PIPE_SIZE)
            p->head = 0;
    }

    p->len += (unsigned int)n;
    local_irq_enable();

    wake_wait_queue(&p->read_wait);
    return (long)n;
}

static int pipe_release(struct file *file)
{
    struct pipe *p = file->private_data;

    if (!p)
        return 0;

    local_irq_disable();

    if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
        if (p->readers > 0)
            p->readers--;
    } else {
        if (p->writers > 0)
            p->writers--;
    }

    if (p->readers == 0)
        wake_wait_queue(&p->write_wait);

    if (p->writers == 0)
        wake_wait_queue(&p->read_wait);

    if (p->readers == 0 && p->writers == 0) {
        local_irq_enable();
        free_pages(p, 0);
        return 0;
    }

    local_irq_enable();
    return 0;
}

static struct file_ops pipe_read_ops = {
    .read = pipe_read,
    .release = pipe_release,
};

static struct file_ops pipe_write_ops = {
    .write = pipe_write,
    .release = pipe_release,
};

long ksys_pipe2(int *fildes, int flags)
{
    struct task_struct *task = current;
    struct pipe *p;
    struct file *rfile;
    struct file *wfile;
    int fds[2];
    int fd0;
    int fd1;

    if (!task || !task->is_user)
        return -EINVAL;

    if (!fildes)
        return -EFAULT;

    if (flags != 0)
        return -EINVAL;

    p = alloc_pages(0);
    if (!p)
        return -ENOMEM;

    p->head = 0;
    p->tail = 0;
    p->len = 0;
    p->readers = 1;
    p->writers = 1;
    p->read_wait = NULL;
    p->write_wait = NULL;

    rfile = alloc_file();
    if (!rfile) {
        free_pages(p, 0);
        return -ENOMEM;
    }

    wfile = alloc_file();
    if (!wfile) {
        rfile->f_op = NULL;
        rfile->private_data = NULL;
        fput(rfile);
        free_pages(p, 0);
        return -ENOMEM;
    }

    rfile->f_op = &pipe_read_ops;
    rfile->private_data = p;
    rfile->f_flags = O_RDONLY;

    wfile->f_op = &pipe_write_ops;
    wfile->private_data = p;
    wfile->f_flags = O_WRONLY;

    fd0 = install_fd(task, rfile);
    if (fd0 < 0) {
        fput(rfile);
        fput(wfile);
        return fd0;
    }

    fd1 = install_fd(task, wfile);
    if (fd1 < 0) {
        task->files[fd0] = NULL;
        fput(rfile);
        fput(wfile);
        return fd1;
    }

    fds[0] = fd0;
    fds[1] = fd1;

    if (copy_to_user(fildes, fds, sizeof(fds))) {
        task->files[fd0] = NULL;
        task->files[fd1] = NULL;
        fput(rfile);
        fput(wfile);
        return -EFAULT;
    }

    return 0;
}

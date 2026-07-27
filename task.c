/*
 * Task management — init (PID 1) and kernel threads.
 */

#include "task.h"
#include "sched.h"
#include "time.h"
#include "uart.h"
#include "ramfs.h"

extern void task_trampoline(void);

struct task_struct init_task;
static unsigned long init_stack[INIT_STACK_SIZE / sizeof(unsigned long)];
static unsigned long next_pid = 1;

__attribute__((noinline))
static void task_frame_init(struct task_struct *task, void (*fn)(void *), void *arg)
{
    unsigned long *sp = task->stack + (INIT_STACK_SIZE / sizeof(unsigned long)) - 12;
    unsigned int i;

    for (i = 0; i < 12; i++)
        sp[i] = 0;

    sp[0] = (unsigned long)arg;
    sp[1] = (unsigned long)fn;
    sp[11] = (unsigned long)task_trampoline;
    task->saved_sp = sp;
}

struct task_struct *kernel_thread(void (*fn)(void *), void *arg)
{
    init_task.pid = next_pid++;
    init_task.state = TASK_SLEEPING;
    init_task.thread_fn = fn;
    init_task.thread_arg = arg;
    init_task.next = 0;
    init_task.stack = init_stack;
    init_task.saved_sp = 0;

    task_frame_init(&init_task, fn, arg);
    return &init_task;
}

void wake_up_process(struct task_struct *task)
{
    task->state = TASK_RUNNING;
}

static void ramfs_list_entry(const char *name, struct ramfs_inode *inode, void *arg)
{
    (void)arg;

    uart_puts("  ");
    uart_puts(name);
    uart_puts(ramfs_is_dir(inode) ? "/\n" : "\n");
}

void initramfs_show(void)
{
    struct ramfs_inode *motd;
    char buf[64];
    long n;

    uart_puts("rootfs listing:\n");
    ramfs_readdir(ramfs_root(), ramfs_list_entry, 0);

    motd = ramfs_lookup("/etc/motd");
    if (!motd)
        return;

    n = ramfs_read(motd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
        return;

    buf[n] = '\0';
    uart_puts("/etc/motd: ");
    uart_puts(buf);
}


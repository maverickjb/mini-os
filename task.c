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

static void ramfs_self_test(void)
{
    struct ramfs_inode *file;
    char buf[32];
    long n;

    uart_puts("ramfs: mounted at /\n");

    if (ramfs_mkdir("/tmp") != 0) {
        uart_puts("ramfs: mkdir /tmp failed\n");
        return;
    }

    if (ramfs_create("/tmp/hello.txt") != 0) {
        uart_puts("ramfs: create failed\n");
        return;
    }

    file = ramfs_lookup("/tmp/hello.txt");
    if (!file) {
        uart_puts("ramfs: lookup failed\n");
        return;
    }

    ramfs_write(file, "Hello ramfs\n", 12, 0);
    n = ramfs_read(file, buf, sizeof(buf) - 1, 0);
    if (n < 0)
        return;

    buf[n] = '\0';
    uart_puts("ramfs: read /tmp/hello.txt: ");
    uart_puts(buf);

    uart_puts("ramfs: listing /tmp:\n");
    ramfs_readdir(ramfs_lookup("/tmp"), ramfs_list_entry, 0);
}

void kernel_init(void *arg)
{
    (void)arg;

    uart_puts("Init (PID 1) running\n");

    ramfs_self_test();

    time_init();
    uart_puts("Tick timer started\n");

    for (;;) {
        static unsigned int beats;

        if ((beats++ % 20) == 0)
            uart_puts("init: heartbeat\n");
        for (volatile unsigned int i = 0; i < 10000000U; i++)
            ;
        schedule();
    }
}

/*
 * Task management — init (PID 1) and kernel threads.
 */

#include "task.h"
#include "sched.h"
#include "time.h"
#include "uart.h"

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

void kernel_init(void *arg)
{
    (void)arg;

    uart_puts("Init (PID 1) running\n");

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

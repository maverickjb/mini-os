#ifndef __LINUX_SCHED_H
#define __LINUX_SCHED_H

#include <linux/mm_types.h>
#include <linux/fs.h>
#include <asm/ptrace.h>
#include <asm/signal.h>

struct dentry;

#define INIT_STACK_SIZE   4096
#define NR_OPEN           8
#define SCHED_TIME_SLICE  10

enum task_state {
    TASK_RUNNING,
    TASK_IDLE,
    TASK_SLEEPING,
    TASK_STOPPED,
    TASK_ZOMBIE,
    TASK_DEAD,
};

enum child_event {
    CHILD_EVENT_NONE,
    CHILD_EVENT_STOPPED,
    CHILD_EVENT_CONTINUED,
};

struct cpu_context {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;
    unsigned long pc;
    unsigned long sp;
};

struct task_struct {
    unsigned long pid;
    enum task_state state;
    struct cpu_context ctx;
    void (*thread_fn)(void *);
    void *thread_arg;
    unsigned long *stack;
    struct mm_struct *mm;
    struct file *files[NR_OPEN];
    struct task_struct *next;
    struct task_struct *parent;
    int time_slice;
    int is_user;
    unsigned long user_sp;
    /*
     * Active trap frame on this task's kernel stack (syscall/IRQ entry),
     * or a fabricated frame at stack top for a newly forked/exec'd task.
     */
    struct pt_regs *regs;
    int exit_code;
    int exit_signal;
    int stop_signal;
    enum child_event wait_event;
    unsigned long pgid;
    /* PSTATE.{D,A,I,F} — saved/restored across switch_to */
    unsigned long daif;
    /* Working directory dentry (NULL means root). */
    struct dentry *cwd;
    /* Pending signals bitmask (bit N => signal N). */
    unsigned long pending;
    unsigned long blocked;
    /* Pre-rt_sigsuspend mask; restored after delivery / syscall return. */
    unsigned long saved_blocked;
    int restore_sigmask;
    struct sigaction actions[MAX_SIG];
};

extern struct task_struct idle_tasks[];
extern struct task_struct *runqueue;
extern struct task_struct *cpu_current_export;

void sched_init(void);
void sched_init_idle(unsigned int cpu);
void cpu_idle(void);
void schedule(void);
void enqueue_task(struct task_struct *task);
void dequeue_task(struct task_struct *task);
struct task_struct *pick_next_task(struct task_struct *prev);

#endif

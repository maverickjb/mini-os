#ifndef __LINUX_SCHED_H
#define __LINUX_SCHED_H

#include <linux/types.h>
#include <linux/mm_types.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <asm/ptrace.h>
#include <asm/signal.h>

struct dentry;

#define INIT_STACK_SIZE   8192
#define NR_OPEN           32
#define SCHED_TIME_SLICE  10

#define TIF_NEED_RESCHED    (1UL << 0) /* reserved; still use need_resched */
#define TIF_POLLING_NRFLAG  (1UL << 1)

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

struct rq {
    spinlock_t lock;
    struct list_head tasks;
    unsigned int nr_running;
	unsigned int cpu;
};

struct sched_stat {
    unsigned long nr_switches;
    unsigned long nr_pulls;
    unsigned long nr_ipis;
};

extern struct sched_stat sched_stat[];

struct task_struct {
    pid_t pid;
    pid_t tgid; /* thread-group id; same as pid until CLONE_THREAD */
    enum task_state state;
    struct cpu_context ctx;
    void (*thread_fn)(void *);
    void *thread_arg;
    unsigned long *stack;
    struct mm_struct *mm;
    struct file *files[NR_OPEN];
    unsigned long close_on_exec; /* bit i => FD_CLOEXEC on files[i] */
	unsigned int cpu;
    /* 1 while this task is executing (or about to) on some CPU. */
    int on_cpu;
    struct list_head run_list;
    struct list_head task_list;
    struct task_struct *parent;
    int time_slice;
    volatile int need_resched;
    unsigned long flags;
    int is_user;
    unsigned long user_sp;
    /* TPIDR_EL0 — musl TLS base; must be saved/restored across switches. */
    unsigned long tpidr_el0;
    /*
     * Active trap frame on this task's kernel stack (syscall/IRQ entry),
     * or a fabricated frame at stack top for a newly forked/exec'd task.
     */
    struct pt_regs *regs;
    int exit_code;
    int exit_signal;
    int stop_signal;
    enum child_event wait_event;
    pid_t pgid;
    pid_t sid;
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
    /* Set around handle_syscall(); orig_x0 is arg0 before it becomes retval. */
    int in_syscall;
    unsigned long orig_x0;
    struct sigaction actions[MAX_SIG];
    int *clear_child_tid;
    /* Jiffies deadline for nanosleep; 0 => not sleeping on a timer. */
    unsigned long wake_jiffies;
    /* /proc/<pid>/stat comm and /proc/<pid>/cmdline (NUL-separated). */
    char comm[16];
    char cmdline[128];
};

extern struct task_struct idle_tasks[];

struct task_struct *get_current(void);

/*
 * Freestanding -O0 has no libgcc outline-atomics. ldxr/stlxr match
 * include/linux/spinlock.h.
 */
static inline __attribute__((always_inline)) void
set_tsk_flag(struct task_struct *p, unsigned long flag)
{
    unsigned long old, n;
    unsigned int t;

    if (!p)
        return;
    do {
        __asm__ volatile("ldxr %0, [%1]" : "=&r"(old) : "r"(&p->flags) : "memory");
        n = old | flag;
        __asm__ volatile("stlxr %w0, %1, [%2]"
                         : "=&r"(t)
                         : "r"(n), "r"(&p->flags)
                         : "memory");
    } while (t);
}

static inline __attribute__((always_inline)) void
clear_tsk_flag(struct task_struct *p, unsigned long flag)
{
    unsigned long old, n;
    unsigned int t;

    if (!p)
        return;
    do {
        __asm__ volatile("ldxr %0, [%1]" : "=&r"(old) : "r"(&p->flags) : "memory");
        n = old & ~flag;
        __asm__ volatile("stlxr %w0, %1, [%2]"
                         : "=&r"(t)
                         : "r"(n), "r"(&p->flags)
                         : "memory");
    } while (t);
}

static inline __attribute__((always_inline)) int
test_tsk_flag(struct task_struct *p, unsigned long flag)
{
    unsigned long v;

    if (!p)
        return 0;
    __asm__ volatile("ldar %0, [%1]" : "=r"(v) : "r"(&p->flags) : "memory");
    return (v & flag) != 0;
}

static inline void set_need_resched(void)
{
    struct task_struct *task = get_current();

    if (task)
        task->need_resched = 1;
}

static inline void clear_need_resched(void)
{
    struct task_struct *task = get_current();

    if (task)
        task->need_resched = 0;
}

static inline int need_resched(void)
{
    struct task_struct *task = get_current();

    return task && task->need_resched;
}

void rq_init(struct rq *rq, unsigned int cpu);
void task_list_lock_irqsave(unsigned long *flags);
void task_list_unlock_irqrestore(unsigned long flags);
struct list_head *task_list_head(void);
void task_attach(struct task_struct *task);
void task_detach(struct task_struct *task);
void sched_block(enum task_state state);
void sched_init(void);
void sched_init_idle(unsigned int cpu);
void cpu_idle(void);
void schedule(void);
void enqueue_task(struct task_struct *task);
void enqueue_task_cpu(struct task_struct *task, unsigned int cpu);
unsigned int select_wake_cpu(struct task_struct *task);
void dequeue_task(struct task_struct *task);
int migrate_task(struct task_struct *task, unsigned int new_cpu);
void dump_rq(struct rq *rq);
void resched_cpu(unsigned int cpu);
int try_pull_task(unsigned int this_cpu);
struct task_struct *pick_next_task(struct rq *rq, struct task_struct *prev);

/* Caller must hold tasklist lock via task_list_lock_irqsave(). */
#define for_each_task(pos, task)                                        \
    list_for_each(pos, task_list_head())                                \
        if ((task = list_entry(pos, struct task_struct, task_list)), 1)

#endif

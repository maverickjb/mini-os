/*
 * Task management — kernel threads, user fork(), return to user.
 */

#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <asm/irqflags.h>
#include <asm/memory.h>

#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/fs.h>

extern void task_trampoline(void);

static pid_t next_pid = 1;

void copy_pt_regs(struct pt_regs *dst, const struct pt_regs *src)
{
    unsigned long *d = (unsigned long *)dst;
    const unsigned long *s = (unsigned long *)src;
    unsigned int i;
    unsigned int n = sizeof(*dst) / sizeof(unsigned long);

    for (i = 0; i < n; i++)
        d[i] = s[i];
}

/*
 * task->regs must already point at a pt_regs on this task's kernel stack.
 * Point cpu_context at ret_from_fork so the first switch_to returns to EL0.
 */
void task_user_ctx_init(struct task_struct *task)
{
    task->ctx.sp = (unsigned long)task->stack + INIT_STACK_SIZE;
    task->ctx.pc = (unsigned long)ret_from_fork;
}

void ret_from_fork(void)
{
    struct task_struct *task = current;

    __asm__ volatile("msr sp_el0, %0" : : "r"(task->user_sp));
    __asm__ volatile("msr tpidr_el0, %0" : : "r"(task->tpidr_el0));
    if (task->mm)
        mm_install(task->mm);

    /* IRQs stay masked; spsr_el1 restores user DAIF on eret. */
    finish_eret(task->regs);
}

static struct pt_regs *task_stack_regs(struct task_struct *task)
{
    /*
     * Kernel SP grows down from the stack top. Put a fabricated trap frame
     * at the bottom so exec/fork setup cannot clobber the live call chain.
     */
    return (struct pt_regs *)task->stack;
}

static void task_zero(struct task_struct *tsk)
{
    unsigned int i;

    tsk->pid = 0;
    tsk->tgid = 0;
    tsk->state = TASK_SLEEPING;
    tsk->thread_fn = NULL;
    tsk->thread_arg = NULL;
    tsk->stack = NULL;
    tsk->mm = NULL;
    INIT_LIST_HEAD(&tsk->run_list);
    INIT_LIST_HEAD(&tsk->task_list);
    tsk->parent = NULL;
    tsk->time_slice = 0;
    tsk->need_resched = 0;
    tsk->is_user = 0;
    tsk->user_sp = 0;
    tsk->tpidr_el0 = 0;
    tsk->regs = NULL;
    tsk->exit_code = 0;
    tsk->exit_signal = 0;
    tsk->stop_signal = 0;
    tsk->wait_event = CHILD_EVENT_NONE;
    tsk->pgid = 0;
    tsk->sid = 0;
    tsk->daif = 0x3c0UL; /* D|A|I|F masked until first switch saves real DAIF */
    tsk->cwd = NULL; /* NULL => root */
    tsk->pending = 0;
    tsk->blocked = 0;
    tsk->saved_blocked = 0;
    tsk->restore_sigmask = 0;
    tsk->clear_child_tid = NULL;
    tsk->wake_jiffies = 0;
    memset(tsk->comm, 0, sizeof(tsk->comm));
    memset(tsk->cmdline, 0, sizeof(tsk->cmdline));
    memset(tsk->actions, 0, sizeof(tsk->actions)); /* SIG_DFL */

    for (i = 0; i < NR_OPEN; i++)
        tsk->files[i] = NULL;
    tsk->close_on_exec = 0;
}

static void task_ctx_init(struct task_struct *task, void (*fn)(void *), void *arg)
{
    task->ctx.x19 = (unsigned long)arg;
    task->ctx.x20 = (unsigned long)fn;
    task->ctx.sp = (unsigned long)(task->stack +
                                   INIT_STACK_SIZE / sizeof(unsigned long));
    task->ctx.pc = (unsigned long)task_trampoline;
}

static void page_zero(unsigned long *page)
{
    unsigned int i;

    for (i = 0; i < (PAGE_SIZE / sizeof(unsigned long)); i++)
        page[i] = 0;
}

static struct mm_struct *mm_init(struct mm_struct *mm)
{
    unsigned long *pgd;

    pgd = alloc_pages(0);
    if (!pgd) {
        kfree(mm);
        return NULL;
    }

    page_zero(pgd);
    mm->pgd = pgd;
    mm->mmap = NULL;
    mm->entry = 0;
    mm->stack_top = 0;
    mm->start_brk = 0;
    mm->brk = 0;
    mm->mmap_base = USER_MMAP_BASE;
    mm->users = 1;
    return mm;
}

struct mm_struct *mm_alloc(void)
{
    struct mm_struct *mm;

    mm = kmalloc(sizeof(*mm));
    if (!mm)
        return NULL;

    return mm_init(mm);
}

struct mm_struct *dup_mm(struct mm_struct *oldmm)
{
    struct mm_struct *mm;

    if (!oldmm || !oldmm->pgd)
        return NULL;

    mm = kmalloc(sizeof(*mm));
    if (!mm)
        return NULL;

    mm->entry = oldmm->entry;
    mm->stack_top = oldmm->stack_top;
    mm->start_brk = oldmm->start_brk;
    mm->brk = oldmm->brk;
    mm->mmap_base = oldmm->mmap_base;
    mm->users = 1;

    mm->pgd = dup_pgtable(oldmm->pgd, 1);
    if (!mm->pgd) {
        kfree(mm);
        return NULL;
    }

    mm->mmap = dup_vma_list(oldmm->mmap);
    if (oldmm->mmap && !mm->mmap) {
        free_user_page_tables(mm->pgd);
        kfree(mm);
        return NULL;
    }

    return mm;
}

static void copy_task_files(struct task_struct *child, struct task_struct *parent)
{
    unsigned int i;

    child->close_on_exec = parent->close_on_exec;

    for (i = 0; i < NR_OPEN; i++) {
        child->files[i] = parent->files[i];
        if (child->files[i])
            get_file(child->files[i]);
    }
}

/*
 * Leaf pages (including the stack) are already privately copied by
 * dup_pgtable_level(). Just inherit the parent's user SP.
 */
static int dup_user_stack(struct task_struct *child, struct task_struct *parent)
{
    if (!child->mm || !parent->mm || !parent->mm->stack_top)
        return -EINVAL;

    child->user_sp = parent->user_sp;
    return 0;
}

struct task_struct *kernel_thread(void (*fn)(void *), void *arg)
{
    struct task_struct *tsk;
    unsigned long *stack;

    tsk = kmalloc(sizeof(*tsk));
    if (!tsk)
        return NULL;

    stack = alloc_pages(1);
    if (!stack) {
        kfree(tsk);
        return NULL;
    }

    task_zero(tsk);
    tsk->pid = next_pid++;
    tsk->tgid = tsk->pid;
    tsk->pgid = tsk->pid;
    tsk->sid = tsk->pid;
    tsk->state = TASK_SLEEPING;
    tsk->thread_fn = fn;
    tsk->thread_arg = arg;
    tsk->stack = stack;

    task_ctx_init(tsk, fn, arg);
    task_attach(tsk);
    return tsk;
}

void wake_up_process(struct task_struct *task)
{
    if (!task)
        return;

    if (task->state == TASK_RUNNING && list_is_linked(&task->run_list))
        return;

    enqueue_task(task);
}

long ksys_fork(struct pt_regs *regs)
{
    struct task_struct *parent = current;
    struct task_struct *child;
    unsigned long *stack;
    int err;

    if (!parent || !parent->is_user)
        return -EINVAL;

    child = kmalloc(sizeof(*child));
    if (!child)
        return -ENOMEM;

    stack = alloc_pages(1);
    if (!stack) {
        kfree(child);
        return -ENOMEM;
    }

    task_zero(child);
    __asm__ volatile("mrs %0, sp_el0" : "=r"(parent->user_sp));

    child->pid = next_pid++;
    child->tgid = child->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;
    child->state = TASK_RUNNING;
    child->time_slice = SCHED_TIME_SLICE;
    child->is_user = 1;
    child->stack = stack;
    child->parent = parent;
    child->exit_signal = SIGCHLD;
    child->mm = dup_mm(parent->mm);
    if (!child->mm) {
        free_pages(stack, 1);
        kfree(child);
        return -ENOMEM;
    }

    err = dup_user_stack(child, parent);
    if (err) {
        mm_put(child->mm);
        free_pages(stack, 1);
        kfree(child);
        return err;
    }

    child->regs = task_stack_regs(child);
    copy_pt_regs(child->regs, regs);
    child->regs->x0 = 0;
    copy_task_files(child, parent);
    child->cwd = parent->cwd;
    child->blocked = parent->blocked;
    /*
     * Capture parent's live TLS pointer — user code may have written
     * TPIDR_EL0 since the last context switch.
     */
    __asm__ volatile("mrs %0, tpidr_el0" : "=r"(parent->tpidr_el0));
    child->tpidr_el0 = parent->tpidr_el0;
    {
        unsigned int i;

        for (i = 0; i < MAX_SIG; i++)
            child->actions[i] = parent->actions[i];
    }
    task_user_ctx_init(child);

    task_attach(child);
    enqueue_task(child);

    regs->x0 = (unsigned long)child->pid;

    return (long)child->pid;
}

void ksys_sched_yield(void)
{
    if (!current || !current->is_user)
        return;

    local_irq_enable();
    schedule();
    local_irq_disable();
}

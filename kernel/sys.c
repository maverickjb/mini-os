/*
 * System call dispatch from EL0.
 */

#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/serial.h>
#include <linux/sched/task.h>
#include <linux/stddef.h>
#include <linux/signal.h>
#include <asm/ptrace.h>
#include <asm/irqflags.h>

void report_el0_fault(struct pt_regs *regs, unsigned long ec)
{
    unsigned long far;

    __asm__ volatile("mrs %0, far_el1" : "=r"(far));

    uart_puts("EL0 fault ec=0x");
    uart_putc("0123456789abcdef"[(ec >> 4) & 0xf]);
    uart_putc("0123456789abcdef"[ec & 0xf]);
    uart_puts(" elr=0x");
    for (int i = 60; i >= 0; i -= 4) {
        unsigned long nibble = (regs->elr_el1 >> i) & 0xfUL;
        uart_putc(nibble < 10 ? '0' + (char)nibble : 'a' + (char)(nibble - 10));
    }
    uart_puts(" far=0x");
    for (int i = 60; i >= 0; i -= 4) {
        unsigned long nibble = (far >> i) & 0xfUL;
        uart_putc(nibble < 10 ? '0' + (char)nibble : 'a' + (char)(nibble - 10));
    }
    uart_puts("\n");
}

static long handle_syscall(struct pt_regs *regs)
{
    switch (regs->x8) {
    case __NR_write:
        return ksys_write(regs->x0, (const char *)regs->x1, regs->x2);
    case __NR_read:
        return ksys_read(regs->x0, (char *)regs->x1, regs->x2);
    case __NR_openat:
        return ksys_openat((int)regs->x0, (const char *)regs->x1,
                           (int)regs->x2, regs->x3);
    case __NR_mkdirat:
        return ksys_mkdirat((int)regs->x0, (const char *)regs->x1,
                            (umode_t)regs->x2);
    case __NR_unlinkat:
        return ksys_unlinkat((int)regs->x0, (const char *)regs->x1,
                             (int)regs->x2);
    case __NR_linkat:
        return ksys_linkat((int)regs->x0, (const char *)regs->x1,
                           (int)regs->x2, (const char *)regs->x3,
                           (int)regs->x4);
    case __NR_chdir:
        return ksys_chdir((const char *)regs->x0);
    case __NR_getcwd:
        return ksys_getcwd((char *)regs->x0, regs->x1);
    case __NR_close:
        return ksys_close(regs->x0);
    case __NR_dup:
        return ksys_dup(regs->x0);
    case __NR_dup3:
        return ksys_dup3(regs->x0, regs->x1, (int)regs->x2);
    case __NR_pipe2:
        return ksys_pipe2((int *)regs->x0, (int)regs->x1);
    case __NR_fstat:
        return ksys_fstat(regs->x0, (struct stat *)regs->x1);
    case __NR_newfstatat:
        return ksys_newfstatat((int)regs->x0, (const char *)regs->x1,
                               (struct stat *)regs->x2, (int)regs->x3);
    case __NR_getdents64:
        return ksys_getdents64(regs->x0, (void *)regs->x1, regs->x2);
    case __NR_clone:
        return ksys_fork(regs);
    case __NR_sched_yield:
        ksys_sched_yield();
        return 0;
    case __NR_exit:
        ksys_exit((long)regs->x0);
        return 0; /* not reached */
    case __NR_wait4:
        return ksys_wait4((long)regs->x0, (int *)regs->x1, (long)regs->x2);
    case __NR_execve:
        return ksys_execve(regs, (const char *)regs->x0,
                           (char *const *)regs->x1,
                           (char *const *)regs->x2);
    case __NR_brk:
        return ksys_brk(regs->x0);
    case __NR_mmap:
        return ksys_mmap(regs->x0, regs->x1, regs->x2, regs->x3,
                         regs->x4, regs->x5);
    case __NR_munmap:
        return ksys_munmap(regs->x0, regs->x1);
    case __NR_kill:
        return ksys_kill((long)regs->x0, (int)regs->x1);
    case __NR_rt_sigaction:
        return ksys_rt_sigaction((int)regs->x0,
                                 (const struct sigaction *)regs->x1,
                                 (struct sigaction *)regs->x2,
                                 regs->x3);
    case __NR_rt_sigprocmask:
        return ksys_rt_sigprocmask((int)regs->x0,
                                   (const sigset_t *)regs->x1,
                                   (sigset_t *)regs->x2,
                                   regs->x3);
    case __NR_getpid:
        return ksys_getpid();
    default:
        return -ENOSYS;
    }
}

void syscall_handler(struct pt_regs *regs)
{
    long ret;

    local_irq_disable();

    if (current)
        current->regs = regs;

    /*
     * rt_sigreturn restores the full interrupted frame, including x0.
     * It must not go through the normal "regs->x0 = ret" path, and a
     * failed restore must not eret back into __restore_rt (infinite loop).
     */
    if (regs->x8 == __NR_rt_sigreturn) {
        if (ksys_rt_sigreturn(regs) < 0)
            ksys_exit(128 + SIGSEGV);
        do_signal(regs);
        return;
    }

    ret = handle_syscall(regs);
    regs->x0 = (unsigned long)ret;

    /*
     * Common return-to-user work. Keep IRQs masked until finish_eret —
     * enabling here races with staging ELR/SPSR for EL0.
     */
    do_signal(regs);
}

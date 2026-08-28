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
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/tick.h>
#include <asm/ptrace.h>
#include <asm/irqflags.h>
#include <asm/memory.h>

static void uart_hex(unsigned long v)
{
    int i;

    uart_puts("0x");
    for (i = 60; i >= 0; i -= 4) {
        unsigned long nibble = (v >> i) & 0xfUL;

        uart_putc(nibble < 10 ? '0' + (char)nibble : 'a' + (char)(nibble - 10));
    }
}

void report_el0_fault(struct pt_regs *regs, unsigned long ec)
{
    unsigned long far;
    unsigned long esr;

    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

    uart_puts("EL0 fault ec=");
    uart_hex(ec);
    uart_puts(" esr=");
    uart_hex(esr);
    uart_puts(" elr=");
    uart_hex(regs->elr_el1);
    uart_puts(" far=");
    uart_hex(far);
    uart_puts("\n");
}

struct new_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

long ksys_uname(void *buf)
{
    struct new_utsname u;

    if (!buf)
        return -EFAULT;

    memset(&u, 0, sizeof(u));
    strscpy(u.sysname, "Linux", sizeof(u.sysname));
    strscpy(u.nodename, "mini-os", sizeof(u.nodename));
    strscpy(u.release, "0.0", sizeof(u.release));
    strscpy(u.version, "mini-os", sizeof(u.version));
    strscpy(u.machine, "aarch64", sizeof(u.machine));

    if (copy_to_user(buf, &u, sizeof(u)))
        return -EFAULT;
    return 0;
}

long ksys_clock_gettime(int clockid, struct timespec *tp)
{
    struct timespec ts;
    unsigned long j;

    (void)clockid;
    if (!tp)
        return -EFAULT;

    j = get_jiffies();
    ts.tv_sec = (long)(j / HZ);
    ts.tv_nsec = (long)(j % HZ) * (1000000000L / HZ);

    if (copy_to_user(tp, &ts, sizeof(ts)))
        return -EFAULT;
    return 0;
}

long ksys_set_tid_address(int *tidptr)
{
    if (!current)
        return -EINVAL;
    current->clear_child_tid = tidptr;
    return (long)current->pid;
}

/* Linux aarch64 struct sysinfo (subset used by BusyBox). */
struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[20 - 2 * sizeof(long) - sizeof(int)];
};

long ksys_sysinfo(void *info)
{
    struct sysinfo si;
    struct task_struct *walk;
    unsigned short procs = 0;

    if (!info)
        return -EFAULT;

    memset(&si, 0, sizeof(si));
    si.uptime = (long)(get_jiffies() / HZ);
    si.mem_unit = 4096;
    si.totalram = PHYS_MEM_SIZE / 4096UL;
    si.freeram = si.totalram / 2UL;

    for (walk = runqueue; walk; walk = walk->next) {
        if (walk->pid > 0 && walk->state != TASK_DEAD)
            procs++;
    }
    si.procs = procs;

    if (copy_to_user(info, &si, sizeof(si)))
        return -EFAULT;
    return 0;
}

static long handle_syscall(struct pt_regs *regs)
{
    switch (regs->x8) {
    case __NR_write:
        return ksys_write(regs->x0, (const char *)regs->x1, regs->x2);
    case __NR_writev:
        return ksys_writev(regs->x0, (const void *)regs->x1, regs->x2);
    case __NR_sendfile:
        return ksys_sendfile(regs->x0, regs->x1, (long *)regs->x2, regs->x3);
    case __NR_ioctl:
        return ksys_ioctl(regs->x0, (unsigned int)regs->x1, regs->x2);
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
    case __NR_utimensat:
        return ksys_utimensat((int)regs->x0, (const char *)regs->x1,
                              (const struct timespec *)regs->x2,
                              (int)regs->x3);
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
    case __NR_exit_group:
        ksys_exit((long)regs->x0);
        return 0;
    case __NR_set_tid_address:
        return ksys_set_tid_address((int *)regs->x0);
    case __NR_uname:
        return ksys_uname((void *)regs->x0);
    case __NR_clock_gettime:
        return ksys_clock_gettime((int)regs->x0, (struct timespec *)regs->x1);
    case __NR_nanosleep:
        return ksys_nanosleep((const struct timespec *)regs->x0,
                              (struct timespec *)regs->x1);
    case __NR_sysinfo:
        return ksys_sysinfo((void *)regs->x0);
    case __NR_lseek:
        return ksys_lseek((unsigned int)regs->x0, (off_t)regs->x1,
                          (unsigned int)regs->x2);
    case __NR_getuid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getegid:
        return 0;
    case __NR_gettid:
        return ksys_getpid();
    case __NR_getppid:
        if (!current || !current->parent)
            return 0;
        return (long)current->parent->pid;
    case __NR_mprotect:
        return ksys_mprotect(regs->x0, regs->x1, regs->x2);
    case __NR_fcntl:
        return 0;
    case __NR_wait4:
        return ksys_wait4((int)regs->x0, (int *)regs->x1, (long)regs->x2);
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
        return ksys_kill((int)regs->x0, (int)regs->x1);
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
    case __NR_rt_sigpending:
        return ksys_rt_sigpending((sigset_t *)regs->x0, regs->x1);
    case __NR_rt_sigsuspend:
        return ksys_rt_sigsuspend((const sigset_t *)regs->x0, regs->x1);
    case __NR_getpid:
        return ksys_getpid();
    case __NR_getpgid:
        if ((long)regs->x0 != 0)
            return -EINVAL;
        return ksys_getpgrp();
    case __NR_setpgid:
        return ksys_setpgid((pid_t)regs->x0, (pid_t)regs->x1);
    case __NR_getsid:
        return ksys_getsid((pid_t)regs->x0);
    case __NR_setsid:
        return ksys_setsid();
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
            do_exit(SIGSEGV);
        do_signal(regs);
        if (current && current->restore_sigmask) {
            current->blocked = current->saved_blocked & ~SIG_UNBLOCKABLE;
            current->restore_sigmask = 0;
        }
        return;
    }

    ret = handle_syscall(regs);
    regs->x0 = (unsigned long)ret;

    /*
     * Common return-to-user work. Keep IRQs masked until finish_eret —
     * enabling here races with staging ELR/SPSR for EL0.
     */
    do_signal(regs);

    if (current && current->restore_sigmask) {
        current->blocked = current->saved_blocked & ~SIG_UNBLOCKABLE;
        current->restore_sigmask = 0;
    }
}

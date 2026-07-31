/*
 * System call dispatch from EL0.
 */

#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <asm/ptrace.h>

void syscall_handler(struct pt_regs *regs)
{
    long ret = 0;

    switch (regs->x8) {
    case __NR_write:
        ret = ksys_write(regs->x0, (const char *)regs->x1, regs->x2);
        break;
    case __NR_clone:
        ret = ksys_fork(regs);
        break;
    case __NR_sched_yield:
        ksys_sched_yield(regs);
        return;
    case __NR_exit:
        ksys_exit(regs, (long)regs->x0);
        return;
    default:
        ret = -ENOSYS;
        break;
    }

    regs->x0 = (unsigned long)ret;
}

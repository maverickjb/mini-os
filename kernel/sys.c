/*
 * System call dispatch from EL0.
 */

#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <asm/ptrace.h>

void syscall_handler(struct pt_regs *regs)
{
    long ret;

    switch (regs->x8) {
    case __NR_write:
        ret = ksys_write(regs->x0, (const char *)regs->x1, regs->x2);
        break;
    default:
        ret = -38; /* ENOSYS */
        break;
    }

    regs->x0 = (unsigned long)ret;
}

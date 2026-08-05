/*
 * System call dispatch from EL0.
 */

#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/serial.h>
#include <asm/ptrace.h>

void report_el0_fault(struct pt_regs *regs, unsigned long ec)
{
    unsigned long far;

    __asm__ volatile("mrs %0, far_el1" : "=r"(far));

    uart_puts("EL0 fault ec=");
    uart_putc('0' + (char)(ec % 10));
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

void syscall_handler(struct pt_regs *regs)
{
    long ret = 0;

    __asm__ volatile("msr daifset, #3");

    switch (regs->x8) {
    case __NR_write:
        ret = ksys_write(regs->x0, (const char *)regs->x1, regs->x2);
        break;
    case __NR_clone:
        ret = ksys_fork(regs);
        break;
    case __NR_sched_yield:
        ksys_sched_yield(regs);
        __asm__ volatile("msr daifclr, #3");
        return;
    case __NR_exit:
        ksys_exit(regs, (long)regs->x0);
        __asm__ volatile("msr daifclr, #3");
        return;
    case __NR_wait4:
        ret = ksys_wait4(regs, (long)regs->x0, (int *)regs->x1,
                         (long)regs->x2);
        break;
    case __NR_execve:
        ret = ksys_execve(regs, (const char *)regs->x0,
                          (char *const *)regs->x1,
                          (char *const *)regs->x2);
        break;
    case __NR_brk:
        ret = ksys_brk(regs->x0);
        break;
    case __NR_mmap:
        ret = ksys_mmap(regs->x0, regs->x1, regs->x2, regs->x3,
                        regs->x4, regs->x5);
        break;
    default:
        ret = -ENOSYS;
        break;
    }

    __asm__ volatile("msr daifclr, #3");
    regs->x0 = (unsigned long)ret;
}

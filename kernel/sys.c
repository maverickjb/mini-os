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
#include <asm/ptrace.h>

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

void syscall_handler(struct pt_regs *regs)
{
    long ret = 0;

    __asm__ volatile("msr daifset, #3");

    if (current)
        current->regs = regs;

    switch (regs->x8) {
    case __NR_write:
        ret = ksys_write(regs->x0, (const char *)regs->x1, regs->x2);
        break;
    case __NR_read:
        ret = ksys_read(regs->x0, (char *)regs->x1, regs->x2);
        break;
    case __NR_openat:
        ret = ksys_openat((int)regs->x0, (const char *)regs->x1,
                          (int)regs->x2, regs->x3);
        break;
    case __NR_mkdirat:
        ret = ksys_mkdirat((int)regs->x0, (const char *)regs->x1,
                           (umode_t)regs->x2);
        break;
    case __NR_close:
        ret = ksys_close(regs->x0);
        break;
    case __NR_dup:
        ret = ksys_dup(regs->x0);
        break;
    case __NR_dup3:
        ret = ksys_dup3(regs->x0, regs->x1, (int)regs->x2);
        break;
    case __NR_pipe2:
        ret = ksys_pipe2((int *)regs->x0, (int)regs->x1);
        break;
    case __NR_fstat:
        ret = ksys_fstat(regs->x0, (struct stat *)regs->x1);
        break;
    case __NR_newfstatat:
        ret = ksys_newfstatat((int)regs->x0, (const char *)regs->x1,
                              (struct stat *)regs->x2, (int)regs->x3);
        break;
    case __NR_getdents64:
        ret = ksys_getdents64(regs->x0, (void *)regs->x1, regs->x2);
        break;
    case __NR_clone:
        ret = ksys_fork(regs);
        break;
    case __NR_sched_yield:
        ksys_sched_yield();
        regs->x0 = 0;
        return;
    case __NR_exit:
        ksys_exit((long)regs->x0);
        return;
    case __NR_wait4:
        ret = ksys_wait4((long)regs->x0, (int *)regs->x1, (long)regs->x2);
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
    case __NR_munmap:
        ret = ksys_munmap(regs->x0, regs->x1);
        break;
    default:
        ret = -ENOSYS;
        break;
    }

    /*
     * Keep IRQs masked until finish_eret. Unmasking here races with the
     * return path: an IRQ can overlay the pt_regs frame or nest after
     * ELR/SPSR are staged for EL0 and corrupt the eventual eret.
     * User DAIF is restored from spsr_el1 on eret.
     */
    regs->x0 = (unsigned long)ret;
}

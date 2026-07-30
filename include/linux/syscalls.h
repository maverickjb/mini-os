#ifndef _LINUX_SYSCALLS_H
#define _LINUX_SYSCALLS_H

#include <asm/ptrace.h>

long ksys_write(unsigned long fd, const char *buf, unsigned long count);
long ksys_fork(struct pt_regs *regs);
void ksys_sched_yield(struct pt_regs *regs);

#endif	/* _LINUX_SYSCALLS_H */

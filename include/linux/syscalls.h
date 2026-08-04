#ifndef _LINUX_SYSCALLS_H
#define _LINUX_SYSCALLS_H

#include <asm/ptrace.h>

long ksys_write(unsigned long fd, const char *buf, unsigned long count);
long ksys_fork(struct pt_regs *regs);
void ksys_sched_yield(struct pt_regs *regs);
void ksys_exit(struct pt_regs *regs, long status);
long ksys_wait4(struct pt_regs *regs, long pid, int *status, long options);
long ksys_execve(struct pt_regs *regs, const char *filename,
                 char *const *argv, char *const *envp);

#endif	/* _LINUX_SYSCALLS_H */

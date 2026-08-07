#ifndef _LINUX_SYSCALLS_H
#define _LINUX_SYSCALLS_H

#include <asm/ptrace.h>

long ksys_write(unsigned long fd, const char *buf, unsigned long count);
long ksys_read(unsigned long fd, char *buf, unsigned long count);
long ksys_open(const char *filename, int flags, unsigned long mode);
long ksys_openat(int dfd, const char *filename, int flags, unsigned long mode);
long ksys_close(unsigned long fd);
long ksys_dup(unsigned long oldfd);
long ksys_dup2(unsigned long oldfd, unsigned long newfd);
long ksys_dup3(unsigned long oldfd, unsigned long newfd, int flags);
long ksys_fork(struct pt_regs *regs);
void ksys_sched_yield(struct pt_regs *regs);
void ksys_exit(struct pt_regs *regs, long status);
long ksys_wait4(struct pt_regs *regs, long pid, int *status, long options);
long ksys_execve(struct pt_regs *regs, const char *filename,
                 char *const *argv, char *const *envp);
long ksys_brk(unsigned long brk);
long ksys_mmap(unsigned long addr, unsigned long len, unsigned long prot,
               unsigned long flags, unsigned long fd, unsigned long off);
long ksys_munmap(unsigned long addr, unsigned long len);

#endif	/* _LINUX_SYSCALLS_H */

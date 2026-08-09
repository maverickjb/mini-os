#ifndef _LINUX_SYSCALLS_H
#define _LINUX_SYSCALLS_H

#include <asm/ptrace.h>
#include <linux/fs.h>

struct stat;

long ksys_write(unsigned long fd, const char *buf, unsigned long count);
long ksys_read(unsigned long fd, char *buf, unsigned long count);
long ksys_open(const char *filename, int flags, unsigned long mode);
long ksys_openat(int dfd, const char *filename, int flags, unsigned long mode);
long ksys_mkdirat(int dfd, const char *filename, umode_t mode);
long ksys_unlinkat(int dfd, const char *filename, int flag);
long ksys_rmdir(const char *pathname);
long ksys_close(unsigned long fd);
long ksys_dup(unsigned long oldfd);
long ksys_dup2(unsigned long oldfd, unsigned long newfd);
long ksys_dup3(unsigned long oldfd, unsigned long newfd, int flags);
long ksys_pipe2(int *fildes, int flags);
long ksys_fstat(unsigned long fd, struct stat *statbuf);
long ksys_newfstatat(int dfd, const char *filename, struct stat *statbuf,
                     int flag);
long ksys_getdents64(unsigned long fd, void *dirp, unsigned long count);
long ksys_fork(struct pt_regs *regs);
void ksys_sched_yield(void);
void ksys_exit(long status);
long ksys_wait4(long pid, int *status, long options);
long ksys_execve(struct pt_regs *regs, const char *filename,
                 char *const *argv, char *const *envp);
long ksys_brk(unsigned long brk);
long ksys_mmap(unsigned long addr, unsigned long len, unsigned long prot,
               unsigned long flags, unsigned long fd, unsigned long off);
long ksys_munmap(unsigned long addr, unsigned long len);

#endif	/* _LINUX_SYSCALLS_H */

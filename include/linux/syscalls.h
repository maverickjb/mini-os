#ifndef _LINUX_SYSCALLS_H
#define _LINUX_SYSCALLS_H

#include <asm/ptrace.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <asm/signal.h>

struct stat;
struct timespec;

long ksys_write(unsigned long fd, const char *buf, unsigned long count);
long ksys_writev(unsigned long fd, const void *iov, unsigned long iovcnt);
long ksys_sendfile(unsigned long out_fd, unsigned long in_fd, long *offset,
                   unsigned long count);
long ksys_read(unsigned long fd, char *buf, unsigned long count);
long ksys_open(const char *filename, int flags, unsigned long mode);
long ksys_openat(int dfd, const char *filename, int flags, unsigned long mode);
long ksys_mkdirat(int dfd, const char *filename, umode_t mode);
long ksys_unlinkat(int dfd, const char *filename, int flag);
long ksys_linkat(int olddfd, const char *oldname, int newdfd,
                 const char *newname, int flags);
long ksys_rmdir(const char *pathname);
long ksys_chdir(const char *filename);
long ksys_getcwd(char *buf, unsigned long size);
long ksys_utimensat(int dfd, const char *filename, const struct timespec *times,
                    int flags);
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
long ksys_mprotect(unsigned long addr, unsigned long len, unsigned long prot);
long ksys_kill(long pid, int sig);
long ksys_rt_sigaction(int sig, const struct sigaction *act,
                       struct sigaction *oldact, unsigned long sigsetsize);
long ksys_rt_sigreturn(struct pt_regs *regs);
long ksys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset,
                         unsigned long sigsetsize);
long ksys_rt_sigpending(sigset_t *set, unsigned long sigsetsize);
long ksys_rt_sigsuspend(const sigset_t *unewset, unsigned long sigsetsize);
long ksys_getpid(void);
long ksys_getpgrp(void);
long ksys_setpgid(pid_t pid, pid_t pgid);
long ksys_getsid(pid_t pid);
long ksys_setsid(void);
long ksys_ioctl(unsigned long fd, unsigned int cmd, unsigned long arg);
long ksys_uname(void *buf);
long ksys_clock_gettime(int clockid, struct timespec *tp);
long ksys_nanosleep(const struct timespec *req, struct timespec *rem);
long ksys_sysinfo(void *info);
long ksys_lseek(unsigned int fd, off_t offset, unsigned int whence);
long ksys_set_tid_address(int *tidptr);

#endif	/* _LINUX_SYSCALLS_H */

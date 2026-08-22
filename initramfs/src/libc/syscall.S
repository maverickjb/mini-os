/*
 * AArch64 syscall wrappers for mini-os userspace.
 *
 * Linux aarch64 ABI: x8 = nr, x0–x5 = args, svc #0, return in x0.
 */

.section .text
.global syscall
.global write
.global read
.global close
.global open
.global _exit
.global brk
.global fork
.global execve
.global waitpid
.global fstat
.global fstatat
.global getdents64
.global mkdirat
.global unlinkat
.global linkat
.global rmdir
.global chdir
.global getcwd
.global pipe
.global kill
.global rt_sigaction
.global rt_sigprocmask
.global rt_sigpending
.global rt_sigsuspend
.global __restore_rt
.global getpid
.global getpgrp
.global setpgid
.global getsid
.global setsid

#define __NR_getcwd 17
#define __NR_mkdirat 34
#define __NR_unlinkat 35
#define __NR_linkat 37
#define __NR_chdir  49
#define __NR_close  57
#define AT_REMOVEDIR 0x200
#define __NR_pipe2 59
#define __NR_getdents64 61
#define __NR_read   63
#define __NR_write  64
#define __NR_newfstatat 79
#define __NR_fstat  80
#define __NR_exit   93
#define __NR_kill   129
#define __NR_rt_sigsuspend 133
#define __NR_rt_sigaction 134
#define __NR_rt_sigprocmask 135
#define __NR_rt_sigpending 136
#define __NR_rt_sigreturn 139
#define __NR_setpgid 154
#define __NR_getpgid 155
#define __NR_getsid 156
#define __NR_setsid 157
#define __NR_getpid 172
#define __NR_brk    214
#define __NR_openat 56
#define __NR_clone  220
#define __NR_execve 221
#define __NR_wait4  260

/*
 * long syscall(long nr, long a0, long a1, long a2, long a3, long a4, long a5)
 */
syscall:
    mov     x8, x0
    mov     x0, x1
    mov     x1, x2
    mov     x2, x3
    mov     x3, x4
    mov     x4, x5
    mov     x5, x6
    svc     #0
    ret

write:
    mov     x8, #__NR_write
    svc     #0
    ret

read:
    mov     x8, #__NR_read
    svc     #0
    ret

close:
    mov     x8, #__NR_close
    svc     #0
    ret

brk:
    mov     x8, #__NR_brk
    svc     #0
    ret

_exit:
    mov     x8, #__NR_exit
    svc     #0
1:  b       1b

/*
 * open(path, flags, mode) — uses openat(AT_FDCWD, ...).
 * Variadic mode is in x2 from C caller.
 */
open:
    mov     x3, x2          /* mode */
    mov     x2, x1          /* flags */
    mov     x1, x0          /* path */
    mov     x0, #-100       /* AT_FDCWD */
    mov     x8, #__NR_openat
    svc     #0
    ret

/* pid_t fork(void) — mini-os: clone(0). */
fork:
    mov     x0, xzr
    mov     x8, #__NR_clone
    svc     #0
    ret

/* int execve(const char *path, char *const argv[], char *const envp[]) */
execve:
    mov     x8, #__NR_execve
    svc     #0
    ret

/* pid_t waitpid(pid_t pid, int *status, int options) → wait4(..., NULL) */
waitpid:
    mov     x3, xzr         /* rusage = NULL */
    mov     x8, #__NR_wait4
    svc     #0
    ret

/* int fstat(int fd, struct stat *st) */
fstat:
    mov     x8, #__NR_fstat
    svc     #0
    ret

/* int fstatat(int dirfd, const char *path, struct stat *st, int flags) */
fstatat:
    mov     x8, #__NR_newfstatat
    svc     #0
    ret

/* long getdents64(int fd, void *dirp, unsigned long count) */
getdents64:
    mov     x8, #__NR_getdents64
    svc     #0
    ret

/* int mkdirat(int dirfd, const char *path, unsigned int mode) */
mkdirat:
    mov     x8, #__NR_mkdirat
    svc     #0
    ret

/* int unlinkat(int dirfd, const char *path, int flags) */
unlinkat:
    mov     x8, #__NR_unlinkat
    svc     #0
    ret

/*
 * int linkat(int olddirfd, const char *oldpath,
 *            int newdirfd, const char *newpath, int flags)
 */
linkat:
    mov     x8, #__NR_linkat
    svc     #0
    ret

/*
 * int rmdir(const char *path)
 * AArch64 has no __NR_rmdir; use unlinkat(AT_FDCWD, path, AT_REMOVEDIR).
 */
rmdir:
    mov     x1, x0          /* path */
    mov     x0, #-100       /* AT_FDCWD */
    mov     x2, #AT_REMOVEDIR
    mov     x8, #__NR_unlinkat
    svc     #0
    ret

/* int chdir(const char *path) */
chdir:
    mov     x8, #__NR_chdir
    svc     #0
    ret

/* long getcwd(char *buf, unsigned long size) — returns length incl. NUL */
getcwd:
    mov     x8, #__NR_getcwd
    svc     #0
    ret

/* int pipe(int pipefd[2]) — pipe2(pipefd, 0) */
pipe:
    mov     x1, xzr
    mov     x8, #__NR_pipe2
    svc     #0
    ret

/* int kill(pid_t pid, int sig) */
kill:
    mov     x8, #__NR_kill
    svc     #0
    ret

/*
 * int rt_sigaction(int sig, const struct sigaction *act,
 *                  struct sigaction *oldact, size_t sigsetsize)
 */
rt_sigaction:
    mov     x8, #__NR_rt_sigaction
    svc     #0
    ret

/*
 * int rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset,
 *                    size_t sigsetsize)
 */
rt_sigprocmask:
    mov     x8, #__NR_rt_sigprocmask
    svc     #0
    ret

/*
 * int rt_sigpending(sigset_t *set, size_t sigsetsize)
 */
rt_sigpending:
    mov     x8, #__NR_rt_sigpending
    svc     #0
    ret

/*
 * int rt_sigsuspend(const sigset_t *unewset, size_t sigsetsize)
 */
rt_sigsuspend:
    mov     x8, #__NR_rt_sigsuspend
    svc     #0
    ret

/* Kernel trampoline: handler `ret` lands here, then rt_sigreturn. */
__restore_rt:
    mov     x8, #__NR_rt_sigreturn
    svc     #0
    b       __restore_rt

/* pid_t getpid(void) */
getpid:
    mov     x8, #__NR_getpid
    svc     #0
    ret

/* pid_t getpgrp(void) — AArch64: getpgid(0) */
getpgrp:
    mov     x0, xzr
    mov     x8, #__NR_getpgid
    svc     #0
    ret

/* int setpgid(pid_t pid, pid_t pgid) */
setpgid:
    mov     x8, #__NR_setpgid
    svc     #0
    ret

/* pid_t getsid(pid_t pid) */
getsid:
    mov     x8, #__NR_getsid
    svc     #0
    ret

/* pid_t setsid(void) */
setsid:
    mov     x8, #__NR_setsid
    svc     #0
    ret

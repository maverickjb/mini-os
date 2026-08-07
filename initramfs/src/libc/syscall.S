/*
 * AArch64 syscall wrappers and CRT entry for mini-os userspace.
 *
 * Linux aarch64 ABI: x8 = nr, x0–x5 = args, svc #0, return in x0.
 */

.section .text
.global _start
.global syscall
.global write
.global read
.global close
.global _exit
.global brk
.global fork
.global execve
.global waitpid

#define __NR_close  57
#define __NR_read   63
#define __NR_write  64
#define __NR_exit   93
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
.global open
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

/* pid_t waitpid(pid_t pid, int *status, int options) → wait4(..., 0) */
waitpid:
    mov     x3, xzr         /* rusage = NULL */
    mov     x8, #__NR_wait4
    svc     #0
    ret

/*
 * CRT: call main(), then exit with its return value.
 */
_start:
    mov     x0, xzr
    mov     x1, xzr
    bl      main
    bl      _exit

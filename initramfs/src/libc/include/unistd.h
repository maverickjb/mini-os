#ifndef _LIBC_UNISTD_H
#define _LIBC_UNISTD_H

#include <stddef.h>

typedef long ssize_t;
typedef long off_t;
typedef int pid_t;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define AT_FDCWD (-100)

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT     0x40
#define O_TRUNC     0x200
#define O_DIRECTORY 0x10000

long syscall(long nr, long a0, long a1, long a2, long a3, long a4, long a5);

ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);
int open(const char *path, int flags, ...);
int close(int fd);
int mkdirat(int dirfd, const char *path, unsigned int mode);
int unlinkat(int dirfd, const char *path, int flags);
int linkat(int olddirfd, const char *oldpath, int newdirfd,
           const char *newpath, int flags);
int rmdir(const char *path);
int chdir(const char *path);
/* Kernel ABI: returns bytes written including NUL, or -errno. */
long getcwd(char *buf, unsigned long size);
void _exit(int status) __attribute__((noreturn));

#define AT_REMOVEDIR 0x200

static inline int mkdir(const char *path, unsigned int mode)
{
    return mkdirat(AT_FDCWD, path, mode);
}

static inline int unlink(const char *path)
{
    return unlinkat(AT_FDCWD, path, 0);
}

static inline int link(const char *oldpath, const char *newpath)
{
    return linkat(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

pid_t fork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
pid_t waitpid(pid_t pid, int *status, int options);

/* Kernel-style: returns current/new program break address. */
void *brk(void *addr);
void *sbrk(long increment);

#endif

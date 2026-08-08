#ifndef _LIBC_SYS_STAT_H
#define _LIBC_SYS_STAT_H

#include <stddef.h>

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned long st_rdev;
    unsigned long __pad1;
    long st_size;
    int st_blksize;
    int __pad2;
    long st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    int __unused[2];
};

#define S_IFMT   0170000
#define S_IFIFO  0010000
#define S_IFCHR  0020000
#define S_IFDIR  0040000
#define S_IFREG  0100000

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

int fstat(int fd, struct stat *st);
int fstatat(int dirfd, const char *path, struct stat *st, int flags);

static inline int stat(const char *path, struct stat *st)
{
    return fstatat(-100 /* AT_FDCWD */, path, st, 0);
}

#endif

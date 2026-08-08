#ifndef _LIBC_DIRENT_H
#define _LIBC_DIRENT_H

struct linux_dirent64 {
    unsigned long long d_ino;
    long long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_REG     8

long getdents64(int fd, void *dirp, unsigned long count);

#endif

#ifndef _LINUX_ERRNO_H
#define _LINUX_ERRNO_H

#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD         10
#define EAGAIN         11
#define EWOULDBLOCK    EAGAIN
#define ENOMEM         12
#define EACCES         13
#define EFAULT         14
#define EBUSY          16
#define EEXIST         17
#define ENODEV         19
#define ENOTDIR        20
#define EISDIR         21
#define EINVAL         22
#define EMFILE         24
#define ENOTTY         25
#define ESPIPE         29
#define ERANGE         34
#define ENAMETOOLONG   36
#define ENOSYS         38
#define ENOTEMPTY      39
#define EPIPE          32
#define ETIMEDOUT     110

/*
 * Kernel-internal: interrupted syscall may be rewound (ELR -= 4) if the
 * handler has SA_RESTART, or converted to -EINTR. Never a userspace errno.
 */
#define ERESTARTSYS     512

#endif

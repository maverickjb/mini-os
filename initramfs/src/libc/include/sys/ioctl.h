#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410

int ioctl(int fd, unsigned long request, ...);

#endif

#ifndef _LINUX_PID_H
#define _LINUX_PID_H

#include <linux/types.h>

long ksys_getpid(void);
long ksys_getpgrp(void);
long ksys_setpgid(pid_t pid, pid_t pgid);
long ksys_getsid(pid_t pid);
long ksys_setsid(void);

#endif /* _LINUX_PID_H */

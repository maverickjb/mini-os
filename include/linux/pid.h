#ifndef _LINUX_PID_H
#define _LINUX_PID_H

typedef int pid_t;

long ksys_getpid(void);
long ksys_getpgrp(void);
long ksys_setpgid(pid_t pid, pid_t pgid);

#endif /* _LINUX_PID_H */

#ifndef _LINUX_SIGNAL_H
#define _LINUX_SIGNAL_H

#include <asm/signal.h>
#include <asm/ptrace.h>

struct task_struct;

struct task_struct *find_task_by_pid(unsigned long pid);
void do_signal(struct pt_regs *regs);
long ksys_kill(long pid, int sig);
long ksys_rt_sigaction(int sig, const struct sigaction *act,
                       struct sigaction *oldact, unsigned long sigsetsize);

#endif /* _LINUX_SIGNAL_H */

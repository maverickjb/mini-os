#ifndef _LINUX_SIGNAL_H
#define _LINUX_SIGNAL_H

#include <asm/signal.h>
#include <asm/ptrace.h>

struct task_struct;

/*
 * Frame pushed on the user stack when delivering a handler.
 * SP_EL0 points here for the handler and for rt_sigreturn.
 */
struct signal_frame {
	struct pt_regs regs;
	unsigned long user_sp;
};

struct task_struct *find_task_by_pid(unsigned long pid);
void do_signal(struct pt_regs *regs);
long ksys_kill(long pid, int sig);
long ksys_rt_sigaction(int sig, const struct sigaction *act,
                       struct sigaction *oldact, unsigned long sigsetsize);
long ksys_rt_sigreturn(struct pt_regs *regs);

#endif /* _LINUX_SIGNAL_H */

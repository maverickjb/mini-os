#ifndef _LINUX_SIGNAL_H
#define _LINUX_SIGNAL_H

#include <asm/signal.h>
#include <asm/ptrace.h>
#include <linux/types.h>

struct task_struct;

/*
 * Frame pushed on the user stack when delivering a handler.
 * SP_EL0 points here for the handler and for rt_sigreturn.
 */
struct signal_frame {
	struct pt_regs regs;
	unsigned long user_sp;
	unsigned long blocked;
};

#define SIG_BIT(sig)		(1UL << (unsigned)(sig))
#define SIG_UNBLOCKABLE		(SIG_BIT(SIGKILL) | SIG_BIT(SIGSTOP))

struct task_struct *find_task_by_pid(pid_t pid);
int signal_pending(struct task_struct *task);
void signal_send(struct task_struct *task, int sig);
void do_signal(struct pt_regs *regs);
long ksys_kill(long pid, int sig);
long ksys_rt_sigaction(int sig, const struct sigaction *act,
                       struct sigaction *oldact, unsigned long sigsetsize);
long ksys_rt_sigreturn(struct pt_regs *regs);
long ksys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset,
                         unsigned long sigsetsize);
long ksys_rt_sigpending(sigset_t *set, unsigned long sigsetsize);
long ksys_rt_sigsuspend(const sigset_t *unewset, unsigned long sigsetsize);

#endif /* _LINUX_SIGNAL_H */

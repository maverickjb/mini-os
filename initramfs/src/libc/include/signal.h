#ifndef _LIBC_SIGNAL_H
#define _LIBC_SIGNAL_H

#include <stddef.h>

#define _NSIG		64
#define _NSIG_BPW	64
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGWINCH	28

typedef struct {
	unsigned long sig[_NSIG_WORDS];
} sigset_t;

typedef void (*sighandler_t)(int);

#define SIG_DFL		((sighandler_t)0)
#define SIG_IGN		((sighandler_t)1)

#define SIG_BLOCK	0
#define SIG_UNBLOCK	1
#define SIG_SETMASK	2

#define SA_NOCLDSTOP	0x00000001
#define SA_NOCLDWAIT	0x00000002
#define SA_SIGINFO	0x00000004
#define SA_RESTORER	0x04000000
#define SA_ONSTACK	0x08000000
#define SA_RESTART	0x10000000
#define SA_NODEFER	0x40000000
#define SA_RESETHAND	0x80000000

struct sigaction {
	sighandler_t sa_handler;
	unsigned long sa_flags;
	void (*sa_restorer)(void);
	sigset_t sa_mask;
};

int kill(int pid, int sig);
int rt_sigaction(int sig, const struct sigaction *act,
                 struct sigaction *oldact, size_t sigsetsize);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset,
                   size_t sigsetsize);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void __restore_rt(void);

static inline int sigemptyset(sigset_t *set)
{
	if (!set)
		return -1;
	set->sig[0] = 0;
	return 0;
}

static inline int sigaddset(sigset_t *set, int sig)
{
	if (!set || sig <= 0 || sig >= _NSIG)
		return -1;
	set->sig[0] |= (1UL << sig);
	return 0;
}

#endif /* _LIBC_SIGNAL_H */

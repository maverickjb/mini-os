#include <signal.h>

extern void __restore_rt(void);

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    struct sigaction kact;

    if (act) {
        kact = *act;
        kact.sa_restorer = __restore_rt;
        kact.sa_flags |= SA_RESTORER;
        act = &kact;
    }

    return rt_sigaction(sig, act, oldact, sizeof(sigset_t));
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    return rt_sigprocmask(how, set, oldset, sizeof(sigset_t));
}

int sigpending(sigset_t *set)
{
    return rt_sigpending(set, sizeof(sigset_t));
}

int sigsuspend(const sigset_t *mask)
{
    return rt_sigsuspend(mask, sizeof(sigset_t));
}

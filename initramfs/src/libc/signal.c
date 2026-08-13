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

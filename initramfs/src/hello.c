#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

static volatile int got_usr1;
static volatile int got_usr2;
static volatile int in_usr1;
static volatile int usr2_nested;

static void sigusr1_mark(int sig)
{
    (void)sig;
    got_usr1 = 1;
}

static void sigusr2_mark(int sig)
{
    (void)sig;
    if (in_usr1)
        usr2_nested = 1;
    got_usr2 = 1;
}

static void sigusr1_mask_usr2(int sig)
{
    (void)sig;
    in_usr1 = 1;
    kill(getpid(), SIGUSR2);
    got_usr1 = 1;
    in_usr1 = 0;
}

static volatile int got_tstp;

static void sigtstp_mark(int sig)
{
    (void)sig;
    got_tstp = 1;
}

static int wait_stopped(pid_t pid, int sig)
{
    int status;
    pid_t w = waitpid(pid, &status, WUNTRACED);

    return w == pid && WIFSTOPPED(status) && WSTOPSIG(status) == sig;
}

static int wait_continued(pid_t pid)
{
    int status;
    pid_t w = waitpid(pid, &status, WCONTINUED);

    return w == pid && WIFCONTINUED(status);
}

int main(void)
{
    struct stat st;
    char buf[512];
    int fd;
    long n;
    long bpos;
    int ret;

    ret = mkdir("/testdir", 0755);
    if (ret < 0) {
        printf("mkdir /testdir failed: %d\n", ret);
        return 1;
    }

    ret = stat("/testdir", &st);
    if (ret < 0 || !S_ISDIR(st.st_mode)) {
        printf("stat /testdir failed: %d\n", ret);
        return 1;
    }
    printf("mkdir /testdir ok ino=%d\n", (int)st.st_ino);

    ret = mkdir("/testdir", 0755);
    if (ret != -EEXIST) {
        printf("mkdir again expected %d got %d\n", -EEXIST, ret);
        return 1;
    }
    printf("mkdir EEXIST ok\n");

    fd = open("/", O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        printf("open / failed: %d\n", fd);
        return 1;
    }

    printf("getdents64 /:\n");
    for (;;) {
        n = getdents64(fd, buf, sizeof(buf));
        if (n < 0) {
            printf("getdents64 failed: %d\n", (int)n);
            close(fd);
            return 1;
        }
        if (n == 0)
            break;

        for (bpos = 0; bpos < n;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);

            printf("  %s type=%d\n", d->d_name, (int)d->d_type);
            bpos += d->d_reclen;
        }
    }

    close(fd);
    printf("mkdirat ok\n");

    fd = open("/tmpfile", O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        printf("create /tmpfile failed: %d\n", fd);
        return 1;
    }
    close(fd);

    ret = link("/tmpfile", "/tmpfile2");
    if (ret < 0) {
        printf("link failed: %d\n", ret);
        return 1;
    }

    {
        struct stat st2;
        unsigned long ino;

        ret = stat("/tmpfile", &st);
        if (ret < 0) {
            printf("stat /tmpfile after link failed: %d\n", ret);
            return 1;
        }
        ino = st.st_ino;
        if (st.st_nlink != 2) {
            printf("link nlink expected 2 got %u\n", st.st_nlink);
            return 1;
        }

        ret = stat("/tmpfile2", &st2);
        if (ret < 0 || st2.st_ino != ino || st2.st_nlink != 2) {
            printf("link /tmpfile2 mismatch\n");
            return 1;
        }
    }

    ret = unlink("/tmpfile");
    if (ret < 0) {
        printf("unlink /tmpfile failed: %d\n", ret);
        return 1;
    }

    ret = stat("/tmpfile", &st);
    if (ret != -ENOENT) {
        printf("unlink expected ENOENT got %d\n", ret);
        return 1;
    }

    ret = stat("/tmpfile2", &st);
    if (ret < 0 || st.st_nlink != 1) {
        printf("after unlink nlink expected 1\n");
        return 1;
    }

    ret = unlink("/tmpfile2");
    if (ret < 0) {
        printf("unlink /tmpfile2 failed: %d\n", ret);
        return 1;
    }

    printf("link ok\n");

    /* unlink() is for files only; directories need rmdir(). */
    ret = unlink("/testdir");
    if (ret != -EISDIR) {
        printf("unlink(/testdir) should fail with EISDIR, got %d\n", ret);
        return 1;
    }

    ret = mkdir("/cdtest", 0755);
    if (ret < 0) {
        printf("mkdir /cdtest failed: %d\n", ret);
        return 1;
    }

    ret = chdir("/cdtest");
    if (ret < 0) {
        printf("chdir /cdtest failed: %d\n", ret);
        return 1;
    }

    {
        char cwd[256];
        long n;

        n = getcwd(cwd, sizeof(cwd));
        if (n < 0) {
            printf("getcwd failed: %d\n", (int)n);
            return 1;
        }
        if (strcmp(cwd, "/cdtest") != 0) {
            printf("getcwd got '%s' expected /cdtest\n", cwd);
            return 1;
        }
    }

    fd = open("here", O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        printf("open relative here failed: %d\n", fd);
        return 1;
    }
    close(fd);

    ret = stat("/cdtest/here", &st);
    if (ret < 0) {
        printf("stat /cdtest/here failed: %d\n", ret);
        return 1;
    }

    ret = chdir("./.");
    if (ret < 0) {
        printf("chdir ./. failed: %d\n", ret);
        return 1;
    }

    {
        char cwd[256];
        long n;

        n = getcwd(cwd, sizeof(cwd));
        if (n < 0 || strcmp(cwd, "/cdtest") != 0) {
            printf("chdir ./. cwd bad\n");
            return 1;
        }
    }

    ret = chdir("..");
    if (ret < 0) {
        printf("chdir .. failed: %d\n", ret);
        return 1;
    }

    ret = stat("cdtest/../cdtest/here", &st);
    if (ret < 0) {
        printf("stat with .. failed: %d\n", ret);
        return 1;
    }

    ret = unlink("cdtest/here");
    if (ret < 0) {
        printf("unlink cdtest/here failed: %d\n", ret);
        return 1;
    }

    ret = rmdir("cdtest");
    if (ret < 0) {
        printf("rmdir cdtest failed: %d\n", ret);
        return 1;
    }

    ret = rmdir("/testdir");
    if (ret < 0) {
        printf("rmdir /testdir failed: %d\n", ret);
        return 1;
    }

    ret = stat("/testdir", &st);
    if (ret != -ENOENT) {
        printf("rmdir expected ENOENT got %d\n", ret);
        return 1;
    }

    printf("unlink/rmdir/chdir ok\n");

    {
        pid_t pg;
        pid_t cpid;
        pid_t w;
        int status;

        pg = getpgrp();
        if (pg <= 0) {
            printf("getpgrp failed: %d\n", (int)pg);
            return 1;
        }

        cpid = fork();
        if (cpid < 0) {
            printf("fork for getpgrp failed\n");
            return 1;
        }
        if (cpid == 0) {
            if (getpgrp() != pg)
                _exit(1);
            _exit(0);
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("getpgrp child mismatch\n");
            return 1;
        }
    }

    printf("getpgrp ok\n");

    {
        pid_t me;
        pid_t cpid;
        pid_t w;
        int status;

        me = getpid();
        if (setpgid(0, 0) != 0) {
            printf("setpgid(0, 0) failed\n");
            return 1;
        }
        if (getpgrp() != me) {
            printf("setpgid expected pgid %d got %d\n", (int)me, (int)getpgrp());
            return 1;
        }
        if (setpgid(0, 99999) != -EPERM) {
            printf("setpgid nonexistent group expected EPERM\n");
            return 1;
        }

        cpid = fork();
        if (cpid < 0) {
            printf("fork for setpgid failed\n");
            return 1;
        }
        if (cpid == 0) {
            if (setpgid(0, 0) != 0)
                _exit(1);
            if (getpgrp() != getpid())
                _exit(2);
            _exit(0);
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("setpgid child expected 0 got %d\n", status);
            return 1;
        }

        /* Join: child leaves, parent puts it into the parent's group. */
        {
            int ready[2];
            int go[2];
            char c;

            if (pipe(ready) < 0 || pipe(go) < 0) {
                printf("pipe for setpgid join failed\n");
                return 1;
            }
            cpid = fork();
            if (cpid < 0) {
                printf("fork for setpgid join failed\n");
                return 1;
            }
            if (cpid == 0) {
                close(ready[0]);
                close(go[1]);
                if (setpgid(0, 0) != 0)
                    _exit(1);
                write(ready[1], "r", 1);
                if (read(go[0], &c, 1) != 1)
                    _exit(2);
                if (getpgrp() != me)
                    _exit(3);
                _exit(0);
            }
            close(ready[1]);
            close(go[0]);
            if (read(ready[0], &c, 1) != 1) {
                printf("setpgid join ready failed\n");
                return 1;
            }
            if (setpgid(cpid, me) != 0) {
                printf("setpgid join failed\n");
                return 1;
            }
            write(go[1], "g", 1);
            w = waitpid(cpid, &status, 0);
            close(ready[0]);
            close(go[1]);
            if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                printf("setpgid join expected 0 got %d\n", status);
                return 1;
            }
        }
    }

    printf("setpgid ok\n");

    {
        pid_t me;
        pid_t cpid;
        pid_t w;
        pid_t sid;
        int status;

        me = getpid();
        sid = getsid(0);
        if (sid <= 0) {
            printf("getsid(0) failed: %d\n", (int)sid);
            return 1;
        }
        if (getsid(me) != sid) {
            printf("getsid(self) mismatch\n");
            return 1;
        }
        if (getsid(99999) != -ESRCH) {
            printf("getsid missing expected ESRCH\n");
            return 1;
        }

        /* hello already called setpgid(0, 0); group leader cannot setsid. */
        if (setsid() != -EPERM) {
            printf("setsid as pgrp leader expected EPERM\n");
            return 1;
        }

        cpid = fork();
        if (cpid < 0) {
            printf("fork for setsid failed\n");
            return 1;
        }
        if (cpid == 0) {
            pid_t newsid;

            if (getsid(0) != sid)
                _exit(1);
            newsid = setsid();
            if (newsid != getpid())
                _exit(2);
            if (getpgrp() != newsid || getsid(0) != newsid)
                _exit(3);
            if (setsid() != -EPERM)
                _exit(4);
            _exit(0);
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("setsid child expected 0 got %d\n", status);
            return 1;
        }
        if (getsid(cpid) != -ESRCH) {
            printf("getsid after child exit expected ESRCH\n");
            return 1;
        }
    }

    printf("setsid ok\n");
    printf("getsid ok\n");

    {
        int fds[2];
        pid_t cpid;
        pid_t w;
        int status;
        char c;

        if (pipe(fds) < 0) {
            printf("pipe failed\n");
            return 1;
        }

        cpid = fork();
        if (cpid < 0) {
            printf("fork for kill failed\n");
            return 1;
        }

        if (cpid == 0) {
            close(fds[1]);
            read(fds[0], &c, 1);
            _exit(0);
        }

        close(fds[0]);
        close(fds[1]);

        ret = kill(cpid, SIGTERM);
        if (ret < 0) {
            printf("kill failed: %d\n", ret);
            return 1;
        }

        w = waitpid(cpid, &status, 0);
        if (w != cpid) {
            printf("waitpid after kill failed\n");
            return 1;
        }
        if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM) {
            printf("kill status expected SIGTERM got %d\n", status);
            return 1;
        }
    }

    printf("kill ok\n");

    {
        int fds[2];
        int ready[2];
        pid_t a;
        pid_t b;
        pid_t w;
        int status;
        int got_a = 0;
        int got_b = 0;
        char c;

        if (pipe(fds) < 0 || pipe(ready) < 0) {
            printf("pipe for kill pgrp failed\n");
            return 1;
        }

        a = fork();
        if (a < 0) {
            printf("fork a for kill pgrp failed\n");
            return 1;
        }
        if (a == 0) {
            close(fds[1]);
            close(ready[0]);
            if (setpgid(0, 0) != 0)
                _exit(1);
            write(ready[1], "a", 1);
            read(fds[0], &c, 1);
            _exit(11);
        }

        if (read(ready[0], &c, 1) != 1) {
            printf("kill pgrp ready a failed\n");
            return 1;
        }

        b = fork();
        if (b < 0) {
            printf("fork b for kill pgrp failed\n");
            return 1;
        }
        if (b == 0) {
            close(fds[1]);
            close(ready[0]);
            if (setpgid(0, a) != 0)
                _exit(2);
            write(ready[1], "b", 1);
            read(fds[0], &c, 1);
            _exit(12);
        }

        if (read(ready[0], &c, 1) != 1) {
            printf("kill pgrp ready b failed\n");
            return 1;
        }
        close(ready[0]);
        close(ready[1]);
        close(fds[0]);

        if (kill(-a, SIGTERM) < 0) {
            printf("kill pgrp failed\n");
            return 1;
        }

        w = waitpid(a, &status, 0);
        if (w == a && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM)
            got_a = 1;
        w = waitpid(b, &status, 0);
        if (w == b && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM)
            got_b = 1;

        close(fds[1]);
        if (!got_a || !got_b) {
            printf("kill pgrp expected SIGTERM on both children\n");
            return 1;
        }
    }

    printf("kill pgrp ok\n");

    {
        int fds[2];
        int ready[2];
        pid_t cpid;
        pid_t w;
        int status;
        char c;
        volatile int i;

        if (pipe(fds) < 0 || pipe(ready) < 0) {
            printf("pipe for SIGSTOP failed\n");
            return 1;
        }

        cpid = fork();
        if (cpid < 0) {
            printf("fork for SIGSTOP failed\n");
            return 1;
        }

        if (cpid == 0) {
            ssize_t n;

            close(fds[1]);
            close(ready[0]);
            write(ready[1], "r", 1);
            close(ready[1]);
            n = read(fds[0], &c, 1);
            if (n == -EINTR)
                _exit(42);
            _exit(3);
        }

        close(fds[0]);
        close(ready[1]);
        if (read(ready[0], &c, 1) != 1) {
            printf("SIGSTOP ready sync failed\n");
            return 1;
        }
        close(ready[0]);
        for (i = 0; i < 100000; i++)
            ;
        if (kill(cpid, SIGSTOP) < 0) {
            printf("kill SIGSTOP failed\n");
            return 1;
        }
        w = waitpid(cpid, &status, WUNTRACED);
        if (w != cpid || !WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
            printf("WIFSTOPPED expected SIGSTOP got %d\n", status);
            return 1;
        }
        w = waitpid(cpid, &status, WNOHANG);
        if (w != 0) {
            printf("stopped child should not be a zombie\n");
            return 1;
        }
        if (kill(cpid, SIGCONT) < 0) {
            printf("kill SIGCONT failed\n");
            return 1;
        }
        w = waitpid(cpid, &status, WCONTINUED);
        if (w != cpid || !WIFCONTINUED(status)) {
            printf("WIFCONTINUED expected, status %d\n", status);
            return 1;
        }
        w = waitpid(cpid, &status, 0);
        close(fds[1]);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("SIGSTOP/SIGCONT expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("SIGSTOP ok\n");
    printf("SIGCONT ok\n");

    {
        int fds[2];
        int ready[2];
        pid_t cpid;
        pid_t w;
        int status;
        char c;
        volatile int i;
        struct sigaction sa;

        if (pipe(fds) < 0 || pipe(ready) < 0) {
            printf("pipe for SIGTSTP failed\n");
            return 1;
        }

        cpid = fork();
        if (cpid < 0) {
            printf("fork for SIGTSTP failed\n");
            return 1;
        }

        if (cpid == 0) {
            ssize_t n;

            close(fds[1]);
            close(ready[0]);
            write(ready[1], "r", 1);
            close(ready[1]);
            n = read(fds[0], &c, 1);
            if (n == -EINTR)
                _exit(42);
            _exit(3);
        }

        close(fds[0]);
        close(ready[1]);
        if (read(ready[0], &c, 1) != 1) {
            printf("SIGTSTP ready sync failed\n");
            return 1;
        }
        close(ready[0]);
        for (i = 0; i < 100000; i++)
            ;
        if (kill(cpid, SIGTSTP) < 0) {
            printf("kill SIGTSTP failed\n");
            return 1;
        }
        if (!wait_stopped(cpid, SIGTSTP)) {
            printf("WIFSTOPPED expected SIGTSTP\n");
            return 1;
        }
        if (kill(cpid, SIGCONT) < 0) {
            printf("kill SIGCONT after SIGTSTP failed\n");
            return 1;
        }
        if (!wait_continued(cpid)) {
            printf("WIFCONTINUED after SIGTSTP expected\n");
            return 1;
        }
        w = waitpid(cpid, &status, 0);
        close(fds[1]);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("SIGTSTP/SIGCONT expected 42 got %d\n", status);
            return 1;
        }

        /* SIGTSTP is catchable, unlike SIGSTOP. */
        if (pipe(fds) < 0 || pipe(ready) < 0) {
            printf("pipe for SIGTSTP handler failed\n");
            return 1;
        }
        cpid = fork();
        if (cpid < 0) {
            printf("fork for SIGTSTP handler failed\n");
            return 1;
        }
        if (cpid == 0) {
            close(fds[1]);
            close(ready[0]);
            sa.sa_handler = sigtstp_mark;
            sa.sa_flags = 0;
            sa.sa_restorer = 0;
            sa.sa_mask.sig[0] = 0;
            if (sigaction(SIGTSTP, &sa, NULL) < 0)
                _exit(1);
            got_tstp = 0;
            write(ready[1], "r", 1);
            close(ready[1]);
            (void)read(fds[0], &c, 1);
            _exit(got_tstp ? 0 : 3);
        }
        close(fds[0]);
        close(ready[1]);
        if (read(ready[0], &c, 1) != 1) {
            printf("SIGTSTP handler ready failed\n");
            return 1;
        }
        close(ready[0]);
        for (i = 0; i < 100000; i++)
            ;
        if (kill(cpid, SIGTSTP) < 0) {
            printf("kill SIGTSTP handler failed\n");
            return 1;
        }
        w = waitpid(cpid, &status, WUNTRACED);
        close(fds[1]);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("SIGTSTP handler expected 0 got %d\n", status);
            return 1;
        }
    }

    printf("SIGTSTP ok\n");

    {
        int fds[2];
        int ready[2];
        pid_t a;
        pid_t b;
        pid_t w;
        int status;
        char c;
        volatile int i;
        int stop_sig;
        int round;

        for (round = 0; round < 2; round++) {
            stop_sig = (round == 0) ? SIGSTOP : SIGTSTP;

            if (pipe(fds) < 0 || pipe(ready) < 0) {
                printf("pipe for job pgrp failed\n");
                return 1;
            }

            a = fork();
            if (a < 0) {
                printf("fork a for job pgrp failed\n");
                return 1;
            }
            if (a == 0) {
                close(fds[1]);
                close(ready[0]);
                if (setpgid(0, 0) != 0)
                    _exit(1);
                write(ready[1], "a", 1);
                read(fds[0], &c, 1);
                _exit(11);
            }

            if (read(ready[0], &c, 1) != 1) {
                printf("job pgrp ready a failed\n");
                return 1;
            }

            b = fork();
            if (b < 0) {
                printf("fork b for job pgrp failed\n");
                return 1;
            }
            if (b == 0) {
                close(fds[1]);
                close(ready[0]);
                if (setpgid(0, a) != 0)
                    _exit(2);
                write(ready[1], "b", 1);
                read(fds[0], &c, 1);
                _exit(12);
            }

            if (read(ready[0], &c, 1) != 1) {
                printf("job pgrp ready b failed\n");
                return 1;
            }
            close(ready[0]);
            close(ready[1]);
            for (i = 0; i < 100000; i++)
                ;

            if (kill(-a, stop_sig) < 0) {
                printf("kill pgrp stop failed sig=%d\n", stop_sig);
                return 1;
            }
            if (!wait_stopped(a, stop_sig) || !wait_stopped(b, stop_sig)) {
                printf("kill pgrp expected both stopped sig=%d\n", stop_sig);
                return 1;
            }

            if (kill(-a, SIGCONT) < 0) {
                printf("kill pgrp SIGCONT failed\n");
                return 1;
            }
            if (!wait_continued(a) || !wait_continued(b)) {
                printf("kill pgrp expected both continued\n");
                return 1;
            }

            close(fds[0]);
            close(fds[1]);
            w = waitpid(a, &status, 0);
            if (w != a || !WIFEXITED(status) || WEXITSTATUS(status) != 11) {
                printf("job pgrp child a expected 11 got %d\n", status);
                return 1;
            }
            w = waitpid(b, &status, 0);
            if (w != b || !WIFEXITED(status) || WEXITSTATUS(status) != 12) {
                printf("job pgrp child b expected 12 got %d\n", status);
                return 1;
            }
        }
    }

    printf("job pgrp ok\n");

    {
        struct sigaction sa, osa;
        int fds[2];
        int ready[2];
        pid_t cpid;
        pid_t w;
        int status;
        char c;
        volatile int i;

        sa.sa_handler = SIG_IGN;
        sa.sa_flags = 0;
        sa.sa_restorer = 0;
        sa.sa_mask.sig[0] = 0;
        if (sigaction(SIGUSR1, &sa, &osa) < 0) {
            printf("sigaction IGN failed\n");
            return 1;
        }
        if (osa.sa_handler != SIG_DFL) {
            printf("sigaction oldact expected DFL\n");
            return 1;
        }
        if (kill(getpid(), SIGUSR1) < 0) {
            printf("self kill IGN failed\n");
            return 1;
        }

        /* User handler: deliver while blocked in read, then sigreturn. */
        if (pipe(fds) < 0 || pipe(ready) < 0) {
            printf("pipe for handler failed\n");
            return 1;
        }
        cpid = fork();
        if (cpid < 0) {
            printf("fork for sigaction handler failed\n");
            return 1;
        }
        if (cpid == 0) {
            ssize_t n;

            close(fds[1]);
            close(ready[0]);
            sa.sa_handler = sigusr1_mark;
            sa.sa_flags = 0;
            sa.sa_restorer = 0;
            sa.sa_mask.sig[0] = 0;
            if (sigaction(SIGUSR1, &sa, NULL) < 0)
                _exit(1);
            got_usr1 = 0;
            write(ready[1], "r", 1);
            close(ready[1]);
            n = read(fds[0], &c, 1);
            if (got_usr1 && n == -EINTR)
                _exit(42);
            _exit(3);
        }
        close(fds[0]);
        close(ready[1]);
        if (read(ready[0], &c, 1) != 1) {
            printf("ready sync failed\n");
            return 1;
        }
        close(ready[0]);
        /* Let the child reach interruptible sleep in read. */
        for (i = 0; i < 100000; i++)
            ;
        if (kill(cpid, SIGUSR1) < 0) {
            printf("kill handler child failed\n");
            return 1;
        }
        w = waitpid(cpid, &status, 0);
        close(fds[1]);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("sigaction handler expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("rt_sigaction ok\n");
    printf("rt_sigreturn ok\n");

    {
        struct sigaction sa;
        pid_t cpid;
        pid_t w;
        int status;

        cpid = fork();
        if (cpid < 0) {
            printf("fork for sa_mask failed\n");
            return 1;
        }
        if (cpid == 0) {
            sa.sa_handler = sigusr2_mark;
            sa.sa_flags = 0;
            sa.sa_restorer = 0;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIGUSR2, &sa, NULL) < 0)
                _exit(1);

            sa.sa_handler = sigusr1_mask_usr2;
            sa.sa_flags = 0;
            sa.sa_restorer = 0;
            sigemptyset(&sa.sa_mask);
            sigaddset(&sa.sa_mask, SIGUSR2);
            if (sigaction(SIGUSR1, &sa, NULL) < 0)
                _exit(1);

            got_usr1 = 0;
            got_usr2 = 0;
            usr2_nested = 0;
            in_usr1 = 0;
            if (kill(getpid(), SIGUSR1) < 0)
                _exit(2);
            if (got_usr1 && got_usr2 && !usr2_nested)
                _exit(42);
            _exit(3);
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("sa_mask expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("sa_mask ok\n");

    {
        struct sigaction sa;
        sigset_t set, old, old2;
        pid_t cpid;
        pid_t w;
        int status;

        cpid = fork();
        if (cpid < 0) {
            printf("fork for sigprocmask failed\n");
            return 1;
        }
        if (cpid == 0) {
            sa.sa_handler = sigusr1_mark;
            sa.sa_flags = 0;
            sa.sa_restorer = 0;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIGUSR1, &sa, NULL) < 0)
                _exit(1);

            got_usr1 = 0;
            sigemptyset(&set);
            sigaddset(&set, SIGUSR1);
            if (sigprocmask(SIG_BLOCK, &set, &old) < 0)
                _exit(2);
            if (old.sig[0] != 0)
                _exit(3);
            if (sigprocmask(SIG_SETMASK, NULL, &old2) < 0)
                _exit(4);
            if (!(old2.sig[0] & (1UL << SIGUSR1)))
                _exit(5);
            if (kill(getpid(), SIGUSR1) < 0)
                _exit(6);
            if (got_usr1)
                _exit(7);
            if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
                _exit(8);
            if (!got_usr1)
                _exit(9);
            _exit(42);
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("sigprocmask expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("rt_sigprocmask ok\n");

    {
        sigset_t set, pending;
        pid_t cpid;
        pid_t w;
        int status;

        cpid = fork();
        if (cpid < 0) {
            printf("fork for sigpending failed\n");
            return 1;
        }
        if (cpid == 0) {
            sigemptyset(&set);
            sigaddset(&set, SIGUSR1);
            if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
                _exit(1);
            if (kill(getpid(), SIGUSR1) < 0)
                _exit(2);
            sigemptyset(&pending);
            if (sigpending(&pending) < 0)
                _exit(3);
            if (pending.sig[0] != 0)
                _exit(4);
            if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
                _exit(5);
            _exit(42);
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("sigpending expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("rt_sigpending ok\n");

    {
        struct sigaction sa;
        sigset_t block, wait, old;
        int ready[2];
        pid_t cpid;
        pid_t w;
        int status;
        char c;
        volatile int i;

        if (pipe(ready) < 0) {
            printf("pipe for sigsuspend failed\n");
            return 1;
        }
        cpid = fork();
        if (cpid < 0) {
            printf("fork for sigsuspend failed\n");
            return 1;
        }
        if (cpid == 0) {
            close(ready[0]);
            sa.sa_handler = sigusr1_mark;
            sa.sa_flags = 0;
            sa.sa_restorer = 0;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIGUSR1, &sa, NULL) < 0)
                _exit(1);

            got_usr1 = 0;
            sigemptyset(&block);
            sigaddset(&block, SIGUSR1);
            if (sigprocmask(SIG_BLOCK, &block, NULL) < 0)
                _exit(2);

            write(ready[1], "r", 1);
            close(ready[1]);

            sigemptyset(&wait);
            if (sigsuspend(&wait) != -EINTR)
                _exit(3);
            if (!got_usr1)
                _exit(4);
            if (sigprocmask(SIG_SETMASK, NULL, &old) < 0)
                _exit(5);
            if (!(old.sig[0] & (1UL << SIGUSR1)))
                _exit(6);
            _exit(42);
        }
        close(ready[1]);
        if (read(ready[0], &c, 1) != 1) {
            printf("sigsuspend ready sync failed\n");
            return 1;
        }
        close(ready[0]);
        for (i = 0; i < 100000; i++)
            ;
        if (kill(cpid, SIGUSR1) < 0) {
            printf("kill sigsuspend child failed\n");
            return 1;
        }
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("sigsuspend expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("rt_sigsuspend ok\n");

    {
        int fds[2];
        pid_t cpid;
        pid_t w;
        int status;
        char c;

        if (pipe(fds) < 0) {
            printf("pipe for WNOHANG failed\n");
            return 1;
        }
        cpid = fork();
        if (cpid < 0) {
            printf("fork for WNOHANG failed\n");
            return 1;
        }
        if (cpid == 0) {
            close(fds[1]);
            read(fds[0], &c, 1);
            _exit(7);
        }
        close(fds[0]);
        w = waitpid(cpid, &status, WNOHANG);
        if (w != 0) {
            printf("WNOHANG expected 0 got %d\n", (int)w);
            return 1;
        }
        close(fds[1]);
        w = waitpid(cpid, &status, 0);
        if (w != cpid || !WIFEXITED(status) || WEXITSTATUS(status) != 7) {
            printf("wait after WNOHANG expected 7 got %d\n", status);
            return 1;
        }
    }

    printf("WNOHANG ok\n");

    {
        pid_t a;
        pid_t b;
        pid_t w;
        int status;
        int got_a = 0;
        int got_b = 0;

        a = fork();
        if (a < 0) {
            printf("fork a for wait(-1) failed\n");
            return 1;
        }
        if (a == 0)
            _exit(11);

        b = fork();
        if (b < 0) {
            printf("fork b for wait(-1) failed\n");
            return 1;
        }
        if (b == 0)
            _exit(22);

        w = waitpid(-1, &status, 0);
        if (w == a && WIFEXITED(status) && WEXITSTATUS(status) == 11)
            got_a = 1;
        else if (w == b && WIFEXITED(status) && WEXITSTATUS(status) == 22)
            got_b = 1;
        else {
            printf("wait(-1) first unexpected pid=%d status=%d\n", (int)w, status);
            return 1;
        }

        w = waitpid(-1, &status, 0);
        if (w == a && WIFEXITED(status) && WEXITSTATUS(status) == 11)
            got_a = 1;
        else if (w == b && WIFEXITED(status) && WEXITSTATUS(status) == 22)
            got_b = 1;
        else {
            printf("wait(-1) second unexpected pid=%d status=%d\n", (int)w, status);
            return 1;
        }

        if (!got_a || !got_b) {
            printf("wait(-1) missed a child\n");
            return 1;
        }

        w = waitpid(-1, &status, WNOHANG);
        if (w > 0) {
            printf("wait(-1) extra child pid=%d\n", (int)w);
            return 1;
        }
    }

    printf("wait(-1) ok\n");
    return 0;
}

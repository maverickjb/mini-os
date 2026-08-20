#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

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
        if (status != 128 + SIGTERM) {
            printf("kill status expected %d got %d\n", 128 + SIGTERM, status);
            return 1;
        }
    }

    printf("kill ok\n");

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
        if (w != cpid || status != 42) {
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
        if (w != cpid || status != 42) {
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
        if (w != cpid || status != 42) {
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
        if (w != cpid || status != 42) {
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
        if (w != cpid || status != 42) {
            printf("sigsuspend expected 42 got %d\n", status);
            return 1;
        }
    }

    printf("rt_sigsuspend ok\n");
    return 0;
}

#include <unistd.h>
#include <sys/wait.h>

static int run_busybox(char *const argv[])
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        write(2, "fork failed\n", 12);
        return 127;
    }
    if (pid == 0) {
        execve("/bin/busybox", argv, (char *[]){ NULL });
        write(2, "exec busybox failed\n", 20);
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0)
        return 127;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 127;
}

int main(void)
{
    int st;
    char *pwd_argv[] = { "busybox", "pwd", NULL };

    st = run_busybox(pwd_argv);
    write(1, "pwd $?=", 7);
    {
        char c = '0' + (char)(st % 10);

        write(1, &c, 1);
        write(1, "\n", 1);
    }

    for (;;)
        ;
}

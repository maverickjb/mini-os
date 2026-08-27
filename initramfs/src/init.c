#include <unistd.h>
#include <sys/wait.h>

static void echo_status(int st)
{
    char c;

    /* Stand-in for shell `echo $?` (statuses 0..9). */
    c = '0' + (char)(st % 10);
    write(1, &c, 1);
    write(1, "\n", 1);
}

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
    char *true_argv[] = { "busybox", "true", NULL };
    char *false_argv[] = { "busybox", "false", NULL };

    /* busybox true; echo $? */
    st = run_busybox(true_argv);
    write(1, "true $?=", 8);
    echo_status(st);

    /* busybox false; echo $? */
    st = run_busybox(false_argv);
    write(1, "false $?=", 9);
    echo_status(st);

    for (;;)
        ;
}

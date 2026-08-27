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

static void echo_status(const char *label, int st)
{
    char c = '0' + (char)(st % 10);
    unsigned long n = 0;

    while (label[n])
        n++;
    write(1, label, n);
    write(1, &c, 1);
    write(1, "\n", 1);
}

int main(void)
{
    int st;
    char *touch_argv[] = { "busybox", "touch", "/touched", NULL };
    char *ls_argv[] = { "busybox", "ls", "-l", "/", NULL };

    st = run_busybox(touch_argv);
    echo_status("touch $?=", st);

    st = run_busybox(ls_argv);
    echo_status("ls $?=", st);

    for (;;)
        ;
}

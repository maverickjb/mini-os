#include <unistd.h>
#include <sys/wait.h>

static char *shell_argv[] = { "ash", "-l", NULL };
static char *shell_envp[] = {
    "PATH=/bin",
    "HOME=/",
    "TERM=linux",
    NULL,
};

int main(void)
{
    for (;;) {
        pid_t pid;
        int status;

        pid = fork();
        if (pid < 0) {
            write(2, "fork failed\n", 12);
            return 1;
        }

        if (pid == 0) {
            execve("/bin/busybox", shell_argv, shell_envp);
            write(2, "exec ash failed\n", 16);
            _exit(127);
        }

        if (waitpid(pid, &status, 0) == pid) {
            write(2, "ash exited, restarting\n", 23);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                break;
        }
    }

    for (;;)
        ;
}

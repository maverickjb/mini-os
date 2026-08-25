#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    char *argv[] = {
        "busybox",
        "echo",
        "hello from busybox",
        NULL
    };
    char *envp[] = {
        NULL
    };
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        execve("/bin/busybox", argv, envp);
        write(2, "exec busybox failed\n", 20);
        _exit(1);
    }

    if (pid > 0)
        waitpid(pid, NULL, 0);

    for (;;)
        ;
}

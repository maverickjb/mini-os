#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    pid_t pid;
    pid_t ret;
    int status;
    char *argv[] = {
        "hello",
        NULL
    };
    char *envp[] = {
        NULL
    };

    pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        execve("/bin/hello", argv, envp);
        printf("exec failed\n");
        exit(1);
    }

    ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        printf("waitpid failed\n");
        exit(1);
    }

    if (WIFEXITED(status))
        printf("child exited status=%d\n", WEXITSTATUS(status));
    else
        printf("child exited status=%d\n", status);

    for (;;)
        ;
}

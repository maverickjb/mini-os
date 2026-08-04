#define __NR_write  64
#define __NR_clone  220
#define __NR_execve 221
#define __NR_exit   93
#define __NR_wait4  260

static long sys_write(long fd, const char *buf, long len)
{
    register long x8 asm("x8") = __NR_write;
    register long x0 asm("x0") = fd;
    register long x1 asm("x1") = (long)buf;
    register long x2 asm("x2") = len;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x8)
        : "memory", "cc");

    return x0;
}

static long sys_fork(void)
{
    register long x8 asm("x8") = __NR_clone;
    register long x0 asm("x0") = 0;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc");

    return x0;
}

static void sys_exit(long status)
{
    register long x8 asm("x8") = __NR_exit;
    register long x0 asm("x0") = status;

    asm volatile(
        "svc #0"
        :
        : "r"(x0), "r"(x8)
        : "memory", "cc");

    for (;;)
        ;
}

static long sys_execve(const char *filename, char *const argv[],
                       char *const envp[])
{
    register long x8 asm("x8") = __NR_execve;
    register long x0 asm("x0") = (long)filename;
    register long x1 asm("x1") = (long)argv;
    register long x2 asm("x2") = (long)envp;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x8)
        : "memory", "cc");

    return x0;
}

static long sys_waitpid(long pid, int *status)
{
    register long x8 asm("x8") = __NR_wait4;
    register long x0 asm("x0") = pid;
    register long x1 asm("x1") = (long)status;
    register long x2 asm("x2") = 0;
    register long x3 asm("x3") = 0;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory", "cc");

    return x0;
}

static void print_num(long n)
{
    char buf[32];
    int i = 0;

    if (n == 0) {
        sys_write(1, "0", 1);
        return;
    }

    if (n < 0) {
        sys_write(1, "-", 1);
        n = -n;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i--)
        sys_write(1, &buf[i], 1);
}

void _start(void)
{
    long pid;
    long ret;
    int status;
    char *argv[] = {
        "hello",
        0
    };
    char *envp[] = {
        0
    };

    pid = sys_fork();
    if (pid < 0) {
        sys_write(1, "fork failed\n", 12);
        sys_exit(1);
    }

    if (pid == 0) {
        sys_execve("/bin/hello", argv, envp);
        sys_write(1, "exec failed\n", 12);
        sys_exit(1);
    }

    ret = sys_waitpid(pid, &status);
    if (ret != pid) {
        sys_write(1, "waitpid failed\n", 15);
        sys_exit(1);
    }

    sys_write(1, "child exited status=", 20);
    print_num(status);
    sys_write(1, "\n", 1);

    for (;;)
        ;
}

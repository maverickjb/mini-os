#define __NR_write  64
#define __NR_execve 221
#define __NR_exit   93

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

void _start(void)
{
    char *argv[] = {
        "hello",
        0
    };
    char *envp[] = {
        0
    };

    sys_execve("/bin/hello", argv, envp);

    sys_write(1, "exec failed\n", 12);

    for (;;)
        ;
}

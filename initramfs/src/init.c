#define __NR_write       64
#define __NR_clone       220
#define __NR_exit        93
#define __NR_sched_yield 124

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

static void sys_exit(void)
{
    register long x8 asm("x8") = __NR_exit;
    register long x0 asm("x0") = 0;

    asm volatile(
        "svc #0"
        :
        : "r"(x0), "r"(x8)
        : "memory", "cc");

    for (;;);
}

static void sys_sched_yield(void)
{
    register long x8 asm("x8") = __NR_sched_yield;
    register long x0 asm("x0") = 0;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc");
}

void _start(void)
{
    long pid;

    sys_write(1, "before fork\n", 12);

    pid = sys_fork();

    if (pid == 0) {
        sys_write(1, "child\n", 6);
        sys_exit();
    }

    sys_write(1, "parent\n", 7);

    for (;;);
}

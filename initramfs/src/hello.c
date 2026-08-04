#define __NR_write 64
#define __NR_exit  93

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

void _start(void)
{
    sys_write(1, "hello from exec\n", 16);
    sys_exit(42);
}

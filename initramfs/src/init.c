#define __NR_write 64

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

void _start(void)
{
    const char msg[] = "hello\n";

    sys_write(1, msg, sizeof(msg) - 1);

    for (;;)
        ;
}

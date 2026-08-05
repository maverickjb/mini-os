#define __NR_write 64
#define __NR_exit  93
#define __NR_brk   214

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

static long sys_brk(unsigned long addr)
{
    register long x8 asm("x8") = __NR_brk;
    register long x0 asm("x0") = (long)addr;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc");

    return x0;
}

static void print_hex(unsigned long n)
{
    int i;

    sys_write(1, "0x", 2);
    for (i = 60; i >= 0; i -= 4) {
        unsigned long nibble = (n >> i) & 0xfUL;
        char c = nibble < 10 ? '0' + (char)nibble : 'a' + (char)(nibble - 10);

        sys_write(1, &c, 1);
    }
}

void _start(void)
{
    unsigned long cur;
    unsigned long next;
    volatile unsigned long *p;
    unsigned long i;

    sys_write(1, "hello from exec\n", 16);

    cur = (unsigned long)sys_brk(0);
    sys_write(1, "brk cur=", 8);
    print_hex(cur);
    sys_write(1, "\n", 1);

    next = (unsigned long)sys_brk(cur + 4096);
    if (next != cur + 4096) {
        sys_write(1, "brk grow failed\n", 16);
        sys_exit(1);
    }

    sys_write(1, "brk new=", 8);
    print_hex(next);
    sys_write(1, "\n", 1);

    p = (volatile unsigned long *)cur;
    for (i = 0; i < 512; i++)
        p[i] = 0xA5A50000UL + i;

    for (i = 0; i < 512; i++) {
        if (p[i] != 0xA5A50000UL + i) {
            sys_write(1, "brk rw failed\n", 14);
            sys_exit(2);
        }
    }

    sys_write(1, "brk ok\n", 7);
    sys_exit(42);
}

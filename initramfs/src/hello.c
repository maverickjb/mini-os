#define __NR_write  64
#define __NR_exit   93
#define __NR_brk    214
#define __NR_munmap 215
#define __NR_mmap   222

#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20

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

static long sys_mmap(unsigned long addr, unsigned long len, unsigned long prot,
                     unsigned long flags, long fd, unsigned long off)
{
    register long x8 asm("x8") = __NR_mmap;
    register long x0 asm("x0") = (long)addr;
    register long x1 asm("x1") = (long)len;
    register long x2 asm("x2") = (long)prot;
    register long x3 asm("x3") = (long)flags;
    register long x4 asm("x4") = fd;
    register long x5 asm("x5") = (long)off;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory", "cc");

    return x0;
}

static long sys_munmap(unsigned long addr, unsigned long len)
{
    register long x8 asm("x8") = __NR_munmap;
    register long x0 asm("x0") = (long)addr;
    register long x1 asm("x1") = (long)len;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
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
    long map;
    long map2;
    long ret;
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

    map = sys_mmap(0, 8192, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map < 0) {
        sys_write(1, "mmap failed\n", 12);
        sys_exit(3);
    }

    sys_write(1, "mmap addr=", 10);
    print_hex((unsigned long)map);
    sys_write(1, "\n", 1);

    p = (volatile unsigned long *)map;
    for (i = 0; i < 1024; i++)
        p[i] = 0xBEEF0000UL + i;

    for (i = 0; i < 1024; i++) {
        if (p[i] != 0xBEEF0000UL + i) {
            sys_write(1, "mmap rw failed\n", 15);
            sys_exit(4);
        }
    }

    sys_write(1, "mmap ok\n", 8);

    ret = sys_munmap((unsigned long)map, 8192);
    if (ret != 0) {
        sys_write(1, "munmap failed\n", 14);
        sys_exit(5);
    }

    sys_write(1, "munmap ok\n", 10);

    map2 = sys_mmap(0, 4096, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map2 < 0) {
        sys_write(1, "mmap2 failed\n", 13);
        sys_exit(6);
    }

    p = (volatile unsigned long *)map2;
    p[0] = 0xCAFEBABEUL;
    if (p[0] != 0xCAFEBABEUL) {
        sys_write(1, "mmap2 rw failed\n", 16);
        sys_exit(7);
    }

    sys_write(1, "mmap2 addr=", 11);
    print_hex((unsigned long)map2);
    sys_write(1, "\n", 1);

    sys_write(1, "munmap test ok\n", 15);
    sys_exit(42);
}

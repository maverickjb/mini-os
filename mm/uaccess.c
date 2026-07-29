#include <linux/uaccess.h>

static void uaccess_enable(void)
{
    unsigned long sctlr;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1UL << 22); /* UAO: EL1 may access EL0 mappings */
    __asm__ volatile("msr sctlr_el1, %0" : : "r"(sctlr));
    __asm__ volatile("isb");
}

unsigned long copy_from_user(void *dst, const void *src, unsigned long len)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    uaccess_enable();

    while (len--)
        *d++ = *s++;

    return 0;
}

unsigned long copy_to_user(void *dst, const void *src, unsigned long len)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    uaccess_enable();

    while (len--)
        *d++ = *s++;

    return 0;
}

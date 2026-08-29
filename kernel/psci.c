/*
 * PSCI 0.2 via HVC — QEMU virt firmware interface.
 */

#include <asm/psci.h>

unsigned long psci_hvc(unsigned long fn, unsigned long arg0,
                       unsigned long arg1, unsigned long arg2)
{
    register unsigned long x0 asm("x0") = fn;
    register unsigned long x1 asm("x1") = arg0;
    register unsigned long x2 asm("x2") = arg1;
    register unsigned long x3 asm("x3") = arg2;

    asm volatile("hvc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3)
                 : "memory");
    return x0;
}

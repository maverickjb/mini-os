/*
 * PSCI 0.2 via HVC — QEMU virt firmware interface.
 */

#include <asm/psci.h>

/* PSCI function IDs (SMC32 / SMC64). */
#define PSCI_0_2_FN_SYSTEM_OFF		0x84000008UL
#define PSCI_0_2_FN_SYSTEM_RESET	0x84000009UL
#define PSCI_0_2_FN64_CPU_ON		0xc4000003UL

static unsigned long psci_invoke(unsigned long fn, unsigned long arg0,
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

int psci_cpu_on(unsigned long cpu, unsigned long entry)
{
    return (int)psci_invoke(PSCI_0_2_FN64_CPU_ON, cpu, entry, 0);
}

void psci_system_off(void)
{
    psci_invoke(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0);

    for (;;)
        asm volatile("wfi");
}

void psci_system_reset(void)
{
    for (;;)
        psci_invoke(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}

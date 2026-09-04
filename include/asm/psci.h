#ifndef _ASM_PSCI_H
#define _ASM_PSCI_H

/*
 * PSCI 0.2 client API (HVC conduit on QEMU virt).
 * Callers use these helpers; the SMC/HVC invoke path stays private.
 */

int psci_cpu_on(unsigned long cpu, unsigned long entry);
void psci_system_off(void) __attribute__((noreturn));
void psci_system_reset(void) __attribute__((noreturn));

#endif /* _ASM_PSCI_H */

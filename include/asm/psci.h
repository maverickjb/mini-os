#ifndef _ASM_PSCI_H
#define _ASM_PSCI_H

#define PSCI_0_2_FN_SYSTEM_OFF   0x84000008UL
#define PSCI_0_2_FN_SYSTEM_RESET 0x84000009UL

unsigned long psci_hvc(unsigned long fn, unsigned long arg0,
                       unsigned long arg1, unsigned long arg2);

#endif /* _ASM_PSCI_H */

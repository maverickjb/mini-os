#ifndef __ASM_EXCEPTION_H
#define __ASM_EXCEPTION_H

#include <asm/ptrace.h>

static inline int interrupted_el0(struct pt_regs *regs)
{
    return regs && (regs->spsr_el1 & 0xf) == 0;
}

#endif /* __ASM_EXCEPTION_H */
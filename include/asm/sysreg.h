#ifndef __ASM_SYSREG_H
#define __ASM_SYSREG_H

/* Exception Class in ESR_ELx[31:26]. */
#define ESR_ELx_EC_SHIFT        26
#define ESR_ELx_EC_MASK         0x3fUL
#define ESR_ELx_EC_IABT_LOW     0x20UL  /* Instruction Abort, lower EL */
#define ESR_ELx_EC_DABT_LOW     0x24UL  /* Data Abort, lower EL */

/* ISS bit for Data Abort: Write not Read. */
#define ESR_ELx_WNR             (1UL << 6)

static inline unsigned long read_esr_el1(void)
{
    unsigned long val;

    __asm__ volatile("mrs %0, esr_el1" : "=r"(val));
    return val;
}

static inline unsigned long read_far_el1(void)
{
    unsigned long val;

    __asm__ volatile("mrs %0, far_el1" : "=r"(val));
    return val;
}

static inline unsigned long esr_get_ec(unsigned long esr)
{
    return (esr >> ESR_ELx_EC_SHIFT) & ESR_ELx_EC_MASK;
}

#endif /* __ASM_SYSREG_H */

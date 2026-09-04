/*
 * GICv2 / GICv3 (QEMU virt) — init_IRQ() and IRQ dispatch.
 */

#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/tick.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/serial.h>
#include <asm/ptrace.h>
#include <asm/memory.h>
#include <asm/exception.h>

#define GICD_VIRT       ((unsigned long)__phys_to_virt(0x08000000UL))
#define GICC_VIRT       ((unsigned long)__phys_to_virt(0x08010000UL))
#define GICR_VIRT       ((unsigned long)__phys_to_virt(0x080A0000UL))
#define GICR_STRIDE     0x20000UL

#define GICD_CTLR       (*(volatile unsigned int *)(GICD_VIRT + 0x0000))
#define GICD_IGROUPR    ((volatile unsigned int *)(GICD_VIRT + 0x0080))
#define GICD_ISENABLER  ((volatile unsigned int *)(GICD_VIRT + 0x0100))
#define GICD_IPRIORITYR ((volatile unsigned char *)(GICD_VIRT + 0x0400))
#define GICD_IROUTER    ((volatile unsigned long *)(GICD_VIRT + 0x6000))
#define GICD_PIDR2      (*(volatile unsigned int *)(GICD_VIRT + 0xFFE8))

#define GICC_CTLR       (*(volatile unsigned int *)(GICC_VIRT + 0x0000))
#define GICC_PMR        (*(volatile unsigned int *)(GICC_VIRT + 0x0004))
#define GICC_IAR        (*(volatile unsigned int *)(GICC_VIRT + 0x000C))
#define GICC_EOIR       (*(volatile unsigned int *)(GICC_VIRT + 0x0010))

static int gic_is_v3;

static unsigned char *gicr_rd_base(unsigned int cpu)
{
    return (unsigned char *)(GICR_VIRT + cpu * GICR_STRIDE);
}

static int gicr_wait_ready(unsigned int cpu)
{
    volatile unsigned int *waker = (volatile unsigned int *)(gicr_rd_base(cpu) + 0x0014);
    unsigned int timeout = 1000000U;

    *waker &= ~0x2U;
    while ((*waker & 0x4U) && --timeout)
        ;
    return timeout ? 0 : -ETIMEDOUT;
}

static int gic_v3_redist_init(unsigned int cpu, unsigned int irq)
{
    unsigned char *sgi = gicr_rd_base(cpu) + 0x10000;
    volatile unsigned int *igroupr0 = (volatile unsigned int *)(sgi + 0x0080);
    volatile unsigned int *isenabler0 = (volatile unsigned int *)(sgi + 0x0100);
    volatile unsigned char *pri = (volatile unsigned char *)(sgi + 0x0400);

    if (gicr_wait_ready(cpu) < 0)
        return -ETIMEDOUT;

    *igroupr0 = 0xffffffffU;
    *isenabler0 = 1U << irq;
    pri[irq] = 0x80;
    return 0;
}

static int gic_v3_dist_init(void)
{
    unsigned int timeout = 1000000U;

    GICD_CTLR = 0x00000000U;
    while ((GICD_CTLR & (1U << 31)) && --timeout)
        ;
    if (timeout == 0)
        return -ETIMEDOUT;

    GICD_CTLR = (1U << 4) | (1U << 5) | (1U << 1) | 1U;
    timeout = 1000000U;
    while ((GICD_CTLR & (1U << 31)) && --timeout)
        ;
    return timeout ? 0 : -ETIMEDOUT;
}

static void gic_v3_cpu_init(void)
{
    unsigned long sre;

    __asm__ volatile("mrs %0, ICC_SRE_EL1" : "=r"(sre));
    sre |= 1;
    __asm__ volatile("msr ICC_SRE_EL1, %0" : : "r"(sre));
    __asm__ volatile("isb");

    __asm__ volatile("msr ICC_PMR_EL1, %0" : : "r"(0xffUL));
    __asm__ volatile("msr ICC_BPR1_EL1, %0" : : "r"(0UL));
    __asm__ volatile("msr ICC_IGRPEN1_EL1, %0" : : "r"(1UL));
    __asm__ volatile("msr ICC_IGRPEN0_EL1, %0" : : "r"(1UL));
    __asm__ volatile("isb");
}

static int gic_v3_init(void)
{
    if (gic_v3_dist_init() < 0)
        return -ETIMEDOUT;

    if (gic_v3_redist_init(0, IRQ_TIMER) < 0)
        return -ETIMEDOUT;

    gic_v3_cpu_init();
    return 0;
}

static void gic_v2_init(void)
{
    GICD_CTLR = 0;
    GICD_IPRIORITYR[IRQ_TIMER] = 0x80;
    GICD_ISENABLER[IRQ_TIMER / 32] = 1U << (IRQ_TIMER % 32);
    GICD_CTLR = 1;

    GICC_PMR = 0xff;
    GICC_CTLR = 1;
}

void init_IRQ(void)
{
    unsigned int archrev = (GICD_PIDR2 >> 4) & 0x7U;

    gic_is_v3 = (archrev >= 3);

    if (gic_is_v3 && gic_v3_init() < 0)
        gic_is_v3 = 0;

    if (!gic_is_v3)
        gic_v2_init();
}

void irq_enable(unsigned int irq)
{
    unsigned int word = irq / 32U;
    unsigned int bit = irq % 32U;

    GICD_IPRIORITYR[irq] = 0x80;
    if (gic_is_v3) {
        GICD_IGROUPR[word] |= (1U << bit);
        GICD_IROUTER[irq] = 0;
        __asm__ volatile("dsb sy");
    }
    GICD_ISENABLER[word] = 1U << bit;
}

void irq_exit(struct pt_regs *regs)
{
    /*
     * Timer (and others) only set need_resched; scheduling happens here
     * once the handler has finished and before returning from the exception.
     * Restrict to EL0 for now — matches userspace-return scheduling.
     */
    if (need_resched() && interrupted_el0(regs))
        schedule();
}

void handle_arch_irq(struct pt_regs *regs)
{
    unsigned int irq;

    if (current)
        current->regs = regs;

    if (gic_is_v3) {
        __asm__ volatile("mrs %0, ICC_IAR1_EL1" : "=r"(irq));
    } else {
        irq = GICC_IAR;
    }

    if (irq == 1023U)
        return;

    if (irq == (unsigned int)IRQ_TIMER)
        handle_arch_tick(regs);
    else if (irq == (unsigned int)IRQ_UART)
        serial_irq();

    if (gic_is_v3) {
        __asm__ volatile("msr ICC_EOIR1_EL1, %0" : : "r"((unsigned long)irq));
    } else {
        GICC_EOIR = irq;
    }

    irq_exit(regs);
}

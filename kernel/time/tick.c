/*
 * Timekeeping — Linux-inspired names on the ARM Generic Timer.
 */

#include <linux/tick.h>
#include <linux/irq.h>
#include <linux/sched/task.h>
#include <linux/serial.h>
#include <asm/exception.h>
#include <asm/irqflags.h>

static unsigned long jiffies;
static unsigned long timer_freq;

static unsigned long read_cntfrq(void)
{
    unsigned long frq;

    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(frq));
    return frq;
}

static void timer_el1_access_enable(void)
{
    unsigned long ctl;

    __asm__ volatile("mrs %0, CNTKCTL_EL1" : "=r"(ctl));
    ctl |= (1UL << 8) | (1UL << 10); /* EL0PTEN | EL0PCTEN */
    __asm__ volatile("msr CNTKCTL_EL1, %0" : : "r"(ctl));
    __asm__ volatile("isb");
}

static void timer_enable(void)
{
    unsigned long ctl = 1; /* enable, interrupt unmasked */

    __asm__ volatile("msr CNTP_CTL_EL0, %0" : : "r"(ctl));
    __asm__ volatile("isb");
}

static void timer_set_delta(unsigned long ticks)
{
    unsigned long cnt, cval;

    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(cnt));
    cval = cnt + ticks;
    __asm__ volatile("msr CNTP_CVAL_EL0, %0" : : "r"(cval));
    __asm__ volatile("isb");
}

void tick_setup(void)
{
    timer_set_delta(timer_freq / HZ);
}

void tick_init(void)
{
    timer_freq = read_cntfrq();
    if (timer_freq == 0)
        timer_freq = 62500000UL;

    timer_el1_access_enable();
    tick_setup();
    timer_enable();
}

void do_timer(void)
{
    jiffies++;
}

unsigned long get_jiffies(void)
{
    return jiffies;
}

void handle_arch_tick(struct pt_regs *regs)
{
    do_timer();

    if ((jiffies % HZ) == 0) {
        unsigned long sec = jiffies / HZ;

        uart_puts("[tick ");
        if (sec >= 10)
            uart_putc('0' + (char)((sec / 10) % 10));
        uart_putc('0' + (char)(sec % 10));
        uart_puts("s]\n");
    }

    if (current && current->pid != 0 && current->state == TASK_RUNNING) {
        current->time_slice--;
        if (current->time_slice <= 0) {
            current->time_slice = SCHED_TIME_SLICE;
            if (current->is_user && interrupted_el0(regs))
                schedule(regs);
        }
    }

    tick_setup();
}

void time_init(void)
{
    extern void vectors_init(void);

    vectors_init();
    init_IRQ();
    tick_init();
    local_irq_enable();
}

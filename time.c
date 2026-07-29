/*
 * Timekeeping — Linux-inspired names on the ARM Generic Timer.
 */

#include "time.h"
#include <linux/irq.h>
#include <linux/serial.h>

static unsigned long jiffies;
static unsigned long timer_freq;

static unsigned long read_cntfrq(void)
{
    unsigned long frq;

    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(frq));
    return frq;
}

static void timer_enable(void)
{
    unsigned long ctl = 1; /* enable, interrupt unmasked */

    __asm__ volatile("msr CNTP_CTL_EL0, %0" : : "r"(ctl));
    __asm__ volatile("isb");
}

static void timer_set_delta(unsigned long ticks)
{
    __asm__ volatile("msr CNTP_TVAL_EL0, %0" : : "r"(ticks));
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

    timer_enable();
    tick_setup();
}

void do_timer(void)
{
    jiffies++;
}

unsigned long get_jiffies(void)
{
    return jiffies;
}

void handle_arch_tick(void)
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

    tick_setup();
}

void time_init(void)
{
    extern void vectors_init(void);

    vectors_init();
    init_IRQ();
    tick_init();
    irq_enable();
}

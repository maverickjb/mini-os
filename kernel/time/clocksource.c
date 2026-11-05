/*
 * Clocksource — ARM generic timer (CNTPCT / CNTFRQ).
 *
 * Free-running cycle counter for interpolating sub-second time.
 * The tick IRQ still uses the same timer; this path only reads it.
 */

#include <linux/timekeeping.h>

static unsigned long cs_hz;

static unsigned long read_cntpct(void)
{
    unsigned long cnt;

    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(cnt));
    return cnt;
}

static unsigned long read_cntfrq(void)
{
    unsigned long frq;

    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(frq));
    return frq;
}

void clocksource_init(void)
{
    cs_hz = read_cntfrq();
    if (cs_hz == 0)
        cs_hz = 62500000UL;
}

unsigned long clocksource_read(void)
{
    return read_cntpct();
}

unsigned long clocksource_hz(void)
{
    return cs_hz;
}

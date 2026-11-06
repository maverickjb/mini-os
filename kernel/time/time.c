/*
 * Wall-clock timekeeping: PL031 RTC seconds plus generic-timer offset.
 */

#include <linux/timekeeping.h>
#include <linux/errno.h>
#include <asm/irqflags.h>
#include <drivers/rtc/pl031.h>

static unsigned int rtc_base_sec;
static unsigned long cs_base;

void timekeeping_init(void)
{
    pl031_init();
    clocksource_init();

    rtc_base_sec = pl031_read_seconds();
    cs_base = clocksource_read();
}

int get_real_time(struct timespec *ts)
{
    unsigned long now;
    unsigned long hz;
    unsigned long delta;
    unsigned long sec;
    unsigned long nsec;

    if (!ts)
        return -EINVAL;

    hz = clocksource_hz();
    now = clocksource_read();
    if (!hz) {
        ts->tv_sec = (long)pl031_read_seconds();
        ts->tv_nsec = 0;
        return 0;
    }

    delta = now - cs_base;
    sec = (unsigned long)rtc_base_sec + delta / hz;
    nsec = (delta % hz) * (unsigned long)NSEC_PER_SEC / hz;

    ts->tv_sec = (long)sec;
    ts->tv_nsec = (long)nsec;
    return 0;
}

int set_real_time(const struct timespec *ts)
{
    unsigned long flags;
    unsigned long hz;
    unsigned long now;
    unsigned long sub;

    if (!ts)
        return -EINVAL;
    if (ts->tv_sec < 0)
        return -EINVAL;
    if (ts->tv_nsec < 0 || ts->tv_nsec >= NSEC_PER_SEC)
        return -EINVAL;

    flags = local_irq_save();
    pl031_write_seconds((unsigned int)ts->tv_sec);
    rtc_base_sec = (unsigned int)ts->tv_sec;
    hz = clocksource_hz();
    now = clocksource_read();
    if (hz)
        sub = (unsigned long)ts->tv_nsec * hz / (unsigned long)NSEC_PER_SEC;
    else
        sub = 0;
    cs_base = now - sub;
    local_irq_restore(flags);
    return 0;
}

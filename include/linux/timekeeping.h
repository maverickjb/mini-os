#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

#include <linux/stat.h>

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

#define NSEC_PER_SEC    1000000000L

void clocksource_init(void);
unsigned long clocksource_read(void);
unsigned long clocksource_hz(void);

void timekeeping_init(void);
int get_real_time(struct timespec *ts);
int set_real_time(const struct timespec *ts);

#endif /* _LINUX_TIMEKEEPING_H */

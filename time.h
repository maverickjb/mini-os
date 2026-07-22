#ifndef TIME_H
#define TIME_H

#define HZ          100

void time_init(void);
void tick_init(void);
void tick_setup(void);
void handle_arch_tick(void);
void do_timer(void);

unsigned long get_jiffies(void);

#endif

#ifndef _LINUX_TICK_H
#define _LINUX_TICK_H

#include <asm/ptrace.h>

#define HZ          100

void time_init(void);
void tick_init(void);
void tick_setup(void);
void handle_arch_tick(struct pt_regs *regs);
void do_timer(void);

unsigned long get_jiffies(void);
void tick_wake_sleepers(void);

#endif /* _LINUX_TICK_H */
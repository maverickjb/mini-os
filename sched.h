#ifndef SCHED_H
#define SCHED_H

#include "fork.h"

void sched_init(void);
void sched_init_idle(unsigned int cpu);
void rest_init(void);
void cpu_idle(void);
void schedule(void);

#endif

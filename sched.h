#ifndef SCHED_H
#define SCHED_H

#include "task.h"

void sched_init(void);
void sched_init_idle(unsigned int cpu);
void rest_init(void);
void cpu_idle(void);
void schedule(void);

#endif

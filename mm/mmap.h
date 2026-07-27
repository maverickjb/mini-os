#ifndef MMAP_H
#define MMAP_H

#include "page_alloc.h"

#define MAP_PROT_READ   (1UL << 0)
#define MAP_PROT_WRITE  (1UL << 1)
#define MAP_PROT_EXEC   (1UL << 2)

int do_map(unsigned long virt, unsigned long phys, unsigned long size,
           unsigned long prot);

#endif

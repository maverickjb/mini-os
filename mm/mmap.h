#ifndef MMAP_H
#define MMAP_H

#include "page_alloc.h"
#include <linux/mm_types.h>

#define MAP_PROT_READ   (1UL << 0)
#define MAP_PROT_WRITE  (1UL << 1)
#define MAP_PROT_EXEC   (1UL << 2)

struct mm_struct *mm_alloc(void);
void mm_get(struct mm_struct *mm);
void mm_put(struct mm_struct *mm);
void mm_install(struct mm_struct *mm);

int do_map(struct mm_struct *mm, unsigned long virt, unsigned long phys,
           unsigned long size, unsigned long prot);

#endif

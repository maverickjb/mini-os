#ifndef __LINUX_MM_H
#define __LINUX_MM_H

#include <linux/mm_types.h>

#define MAP_PROT_READ   (1UL << 0)
#define MAP_PROT_WRITE  (1UL << 1)
#define MAP_PROT_EXEC   (1UL << 2)

struct mm_struct *mm_alloc(void);
struct mm_struct *mm_dup(struct mm_struct *src);
void mm_get(struct mm_struct *mm);
void mm_put(struct mm_struct *mm);
void mm_install(struct mm_struct *mm);

int do_map(struct mm_struct *mm, unsigned long virt, unsigned long phys,
           unsigned long size, unsigned long prot);
long do_brk(struct mm_struct *mm, unsigned long newbrk);

#endif /* __LINUX_MM_H */

#ifndef __LINUX_MM_TYPES_H
#define __LINUX_MM_TYPES_H

#include <linux/atomic.h>

/*
 * Physical page descriptor (mem_map[] entry).
 * One struct page per buddy-pool page; indexed by pool PFN.
 */
struct page {
	atomic_t _refcount;
};

struct vm_area_struct {
    unsigned long vm_start;
    unsigned long vm_end;       /* exclusive */
    unsigned long vm_flags;

    struct vm_area_struct *vm_next;
};

struct mm_struct {
    struct vm_area_struct *mmap;

    unsigned long *pgd;
    unsigned long entry;
    unsigned long stack_top;
    unsigned long start_brk;
    unsigned long brk;
    unsigned long mmap_base;
    int users;
};

#endif	/* __MM_TYPES_H */

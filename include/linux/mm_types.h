#ifndef __LINUX_MM_TYPES_H
#define __LINUX_MM_TYPES_H

struct mm_struct {
    unsigned long *pgd;
    unsigned long entry;
    unsigned long stack_top;
    unsigned long start_brk;
    unsigned long brk;
    int users;
};

#endif	/* __MM_TYPES_H */
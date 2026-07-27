#ifndef MM_TYPES_H
#define MM_TYPES_H

struct mm_struct {
    unsigned long *pgd;
    unsigned long entry;
    unsigned long stack_top;
};

#endif

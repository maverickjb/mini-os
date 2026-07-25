#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)

void page_alloc_init(void);
void *alloc_pages(int order);
void free_pages(void *addr, int order);

#endif

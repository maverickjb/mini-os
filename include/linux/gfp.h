#ifndef __LINUX_GFP_H
#define __LINUX_GFP_H

#include <linux/mm_types.h>

#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE - 1UL))

void page_alloc_init(void);
void *alloc_pages(int order);
void free_pages(void *addr, int order);

struct page *virt_to_page(const void *addr);
void *page_address(const struct page *page);

void get_page(struct page *page);
void put_page(struct page *page);
unsigned int page_count(const struct page *page);

#endif /* __LINUX_GFP_H */

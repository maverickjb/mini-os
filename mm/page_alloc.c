/*
 * Physical page allocator — buddy algorithm.
 *
 * Manages RAM from __alloc_start up to 128 MiB at 0x40000000 (QEMU virt).
 */

#include "page_alloc.h"
#include "mem.h"
#include <linux/stddef.h>

#define MAX_PAGES       (PHYS_MEM_SIZE / PAGE_SIZE)

extern char __alloc_start[];

struct free_block {
    struct free_block *next;
};

static struct free_block *free_area[16];
static unsigned long mem_start;
static unsigned long mem_size;
static unsigned int max_order;
static unsigned int pool_pages;
static signed char block_order[MAX_PAGES];

static unsigned long addr_to_pfn(unsigned long addr)
{
    return (addr - mem_start) >> PAGE_SHIFT;
}

static unsigned long pfn_to_addr(unsigned long pfn)
{
    return mem_start + (pfn << PAGE_SHIFT);
}

static struct free_block *pfn_to_block(unsigned long pfn)
{
    return (struct free_block *)pfn_to_addr(pfn);
}

static int order_valid(int order)
{
    return order >= 0 && (unsigned int)order <= max_order;
}

static int addr_in_pool(unsigned long addr, int order)
{
    unsigned long size = PAGE_SIZE << order;
    unsigned long end = mem_start + mem_size;

    if (addr < mem_start || (addr + size) > end)
        return 0;
    if ((addr & (size - 1)) != 0)
        return 0;
    return 1;
}

static void free_list_add(unsigned long pfn, unsigned int order)
{
    struct free_block *block = pfn_to_block(pfn);

    block->next = free_area[order];
    free_area[order] = block;
    block_order[pfn] = (signed char)order;
}

static void free_list_remove(unsigned long pfn, unsigned int order)
{
    struct free_block *block = pfn_to_block(pfn);
    struct free_block **prev = &free_area[order];

    while (*prev) {
        if (*prev == block) {
            *prev = block->next;
            block_order[pfn] = -1;
            return;
        }
        prev = &(*prev)->next;
    }
}

static unsigned long free_list_pop(unsigned int order)
{
    struct free_block *block = free_area[order];
    unsigned long pfn;

    if (!block)
        return MAX_PAGES;

    free_area[order] = block->next;
    pfn = addr_to_pfn((unsigned long)block);
    block_order[pfn] = -1;
    return pfn;
}

static void split_block(unsigned long pfn, unsigned int from_order,
                        unsigned int to_order)
{
    unsigned int order = from_order;

    while (order > to_order) {
        order--;
        free_list_add(pfn ^ (1UL << order), order);
    }
}

void page_alloc_init(void)
{
    unsigned long start = (unsigned long)__alloc_start;
    unsigned long size;
    unsigned long npages;

    for (unsigned int i = 0; i <= 15; i++)
        free_area[i] = NULL;

    for (unsigned int i = 0; i < MAX_PAGES; i++)
        block_order[i] = -1;

    mem_start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (mem_start >= VIRT_MEM_END)
        return;

    size = VIRT_MEM_END - mem_start;
    npages = size / PAGE_SIZE;
    if (npages == 0)
        return;

    max_order = (unsigned int)(63 - __builtin_clzl(npages));
    pool_pages = 1U << max_order;
    mem_size = (unsigned long)pool_pages << PAGE_SHIFT;

    free_list_add(0, max_order);
}

void *alloc_pages(int order)
{
    unsigned int o;
    unsigned long pfn;

    if (!order_valid(order))
        return NULL;

    for (o = (unsigned int)order; o <= max_order; o++) {
        if (!free_area[o])
            continue;

        pfn = free_list_pop(o);
        if (pfn >= pool_pages)
            return NULL;

        split_block(pfn, o, (unsigned int)order);
        block_order[pfn] = -1;
        return (void *)pfn_to_addr(pfn);
    }

    return NULL;
}

void free_pages(void *addr, int order)
{
    unsigned long pfn;
    unsigned int o;

    if (!addr || !order_valid(order))
        return;

    pfn = addr_to_pfn((unsigned long)addr);
    if (pfn >= pool_pages || !addr_in_pool((unsigned long)addr, order))
        return;
    if (block_order[pfn] >= 0)
        return;

    o = (unsigned int)order;

    while (o < max_order) {
        unsigned long buddy_pfn = pfn ^ (1UL << o);

        if (buddy_pfn >= pool_pages)
            break;
        if (block_order[buddy_pfn] != (signed char)o)
            break;

        free_list_remove(buddy_pfn, o);
        if (buddy_pfn < pfn)
            pfn = buddy_pfn;
        o++;
    }

    free_list_add(pfn, o);
}

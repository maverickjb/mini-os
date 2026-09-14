/*
 * Physical page allocator — buddy algorithm.
 *
 * Manages RAM from __alloc_start up to 128 MiB at 0x40000000 (QEMU virt).
 *
 * Each pool page has a struct page in mem_map[] with atomic _refcount.
 * alloc_pages() sets count to 1; COW will use get_page()/put_page().
 */

#include <linux/gfp.h>
#include <asm/memory.h>
#include <linux/stddef.h>
#include <linux/atomic.h>

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
static struct page mem_map[MAX_PAGES];

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

static int pfn_in_pool(unsigned long pfn)
{
    return pfn < pool_pages;
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

/* Buddy free — caller has already dropped _refcount to 0. */
static void buddy_free(unsigned long pfn, unsigned int order)
{
    unsigned int o = order;

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

void page_alloc_init(void)
{
    unsigned long start = (unsigned long)__alloc_start;
    unsigned long size;
    unsigned long npages;
    unsigned int i;

    for (i = 0; i <= 15; i++)
        free_area[i] = NULL;

    for (i = 0; i < MAX_PAGES; i++) {
        block_order[i] = -1;
        atomic_set(&mem_map[i]._refcount, 0);
    }

    mem_start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (mem_start >= VIRT_MEM_END)
        return;

    size = VIRT_MEM_END - mem_start;
    npages = size / PAGE_SIZE;
    if (npages == 0)
        return;

    max_order = (unsigned int)(63 - __builtin_clzl(npages));
    pool_pages = 1UL << max_order;
    mem_size = pool_pages << PAGE_SHIFT;

    free_list_add(0, max_order);
}

struct page *virt_to_page(const void *addr)
{
    unsigned long pfn;

    if (!addr || !addr_in_pool((unsigned long)addr, 0))
        return NULL;

    pfn = addr_to_pfn((unsigned long)addr);
    if (!pfn_in_pool(pfn))
        return NULL;

    return &mem_map[pfn];
}

void *page_address(const struct page *page)
{
    unsigned long pfn;

    if (!page)
        return NULL;

    pfn = (unsigned long)(page - mem_map);
    if (!pfn_in_pool(pfn))
        return NULL;

    return (void *)pfn_to_addr(pfn);
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
        atomic_set(&mem_map[pfn]._refcount, 1);
        return (void *)pfn_to_addr(pfn);
    }

    return NULL;
}

void free_pages(void *addr, int order)
{
    unsigned long pfn;
    struct page *page;

    if (!addr || !order_valid(order))
        return;

    pfn = addr_to_pfn((unsigned long)addr);
    if (!pfn_in_pool(pfn) || !addr_in_pool((unsigned long)addr, order))
        return;
    if (block_order[pfn] >= 0)
        return;

    page = &mem_map[pfn];
    /*
     * Shared pages (refcount > 1) must use put_page().
     * Direct free_pages() is for exclusive ownership (refcount == 1).
     */
    if (atomic_read(&page->_refcount) > 1)
        return;

    atomic_set(&page->_refcount, 0);
    buddy_free(pfn, (unsigned int)order);
}

void get_page(struct page *page)
{
    if (!page)
        return;

    atomic_inc(&page->_refcount);
}

void put_page(struct page *page)
{
    unsigned long pfn;

    if (!page)
        return;

    if (!atomic_dec_and_test(&page->_refcount))
        return;

    pfn = (unsigned long)(page - mem_map);
    if (!pfn_in_pool(pfn))
        return;

    /* Last reference — return order-0 page to the buddy allocator. */
    buddy_free(pfn, 0);
}

unsigned int page_count(const struct page *page)
{
    if (!page)
        return 0;

    return (unsigned int)atomic_read(&page->_refcount);
}

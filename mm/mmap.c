/*
 * Runtime user mappings via TTBR0 (4 KiB page tables).
 */

#include "mmap.h"
#include <asm/memory.h>
#include "page_alloc.h"
#include <linux/errno.h>
#include <linux/stddef.h>

#define PTE_VALID       3UL
#define PTE_TABLE       3UL
#define PTE_BLOCK       (1UL << 0)
#define PTE_AF          (1UL << 10)
#define PTE_SHARED      (3UL << 8)   /* inner shareable */
#define PTE_USER        (1UL << 6)   /* AP[1]: user accessible at EL0 */
#define PTE_RDONLY      (1UL << 7)   /* AP[2]: read-only */
#define PTE_UXN         (1UL << 54)
#define PTE_ADDR_MASK   (~0xFFFUL)

#define PTE_ENTRIES     512

static void page_zero(unsigned long *page)
{
    unsigned int i;

    for (i = 0; i < PTE_ENTRIES; i++)
        page[i] = 0;
}

static unsigned long prot_to_pte(unsigned long prot)
{
    unsigned long pte = PTE_VALID | PTE_AF | PTE_SHARED | PTE_USER;

    if (!(prot & MAP_PROT_WRITE))
        pte |= PTE_RDONLY;

    if (!(prot & MAP_PROT_EXEC))
        pte |= PTE_UXN;

    return pte;
}

static unsigned long *get_or_create_table(unsigned long *parent, unsigned long index)
{
    unsigned long entry = parent[index];
    unsigned long *table;

    if (entry & 1UL) {
        if ((entry & 3UL) != PTE_TABLE)
            return NULL;
        return (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    }

    table = alloc_pages(0);
    if (!table)
        return NULL;

    page_zero(table);
    parent[index] = __virt_to_phys((unsigned long)table) | PTE_TABLE;
    return table;
}

struct mm_struct *mm_alloc(void)
{
    struct mm_struct *mm;
    unsigned long *pgd;

    mm = alloc_pages(0);
    if (!mm)
        return NULL;

    pgd = alloc_pages(0);
    if (!pgd) {
        free_pages(mm, 0);
        return NULL;
    }

    page_zero(pgd);
    mm->pgd = pgd;
    mm->entry = 0;
    mm->stack_top = 0;
    mm->users = 1;
    return mm;
}

void mm_get(struct mm_struct *mm)
{
    if (mm)
        mm->users++;
}

void mm_put(struct mm_struct *mm)
{
    if (!mm)
        return;

    mm->users--;
    if (mm->users > 0)
        return;

    if (mm->pgd)
        free_pages(mm->pgd, 0);
    free_pages(mm, 0);
}

void mm_install(struct mm_struct *mm)
{
    unsigned long ttbr0;

    if (!mm || !mm->pgd)
        return;

    ttbr0 = __virt_to_phys((unsigned long)mm->pgd);

    __asm__ volatile(
        "msr ttbr0_el1, %0\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        :
        : "r"(ttbr0));
}

static int map_page(struct mm_struct *mm, unsigned long va, unsigned long pa,
                    unsigned long prot)
{
    unsigned long *l2;
    unsigned long *l3;
    unsigned long l1_idx = (va >> 30) & 0x1ffUL;
    unsigned long l2_idx = (va >> 21) & 0x1ffUL;
    unsigned long l3_idx = (va >> 12) & 0x1ffUL;

    l2 = get_or_create_table(mm->pgd, l1_idx);
    if (!l2)
        return -ENOMEM;

    l3 = get_or_create_table(l2, l2_idx);
    if (!l3)
        return -ENOMEM;

    l3[l3_idx] = (pa & PTE_ADDR_MASK) | prot_to_pte(prot);
    __asm__ volatile("dsb sy");
    return 0;
}

int do_map(struct mm_struct *mm, unsigned long virt, unsigned long phys,
           unsigned long size, unsigned long prot)
{
    unsigned long va;
    unsigned long end;
    int err;

    if (!mm || !mm->pgd)
        return -EINVAL;

    if (size == 0)
        return 0;

    va = virt & ~(PAGE_SIZE - 1UL);
    end = (virt + size + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    for (; va < end; va += PAGE_SIZE, phys += PAGE_SIZE) {
        err = map_page(mm, va, phys, prot);
        if (err)
            return err;
    }

    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
    return 0;
}

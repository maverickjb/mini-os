/*
 * Runtime user mappings via TTBR0 (4 KiB page tables).
 */

#include "mmap.h"
#include "mem.h"
#include "page_alloc.h"

#define PTE_VALID       3UL
#define PTE_TABLE       3UL
#define PTE_AF          (1UL << 10)
#define PTE_UXN         (1UL << 54)
#define PTE_AP_RW_EL0   (1UL << 6)
#define PTE_AP_RO_EL0   (3UL << 6)
#define PTE_ADDR_MASK   (~0xFFFUL)

#define PTE_ENTRIES     512

static unsigned long *user_l1;

static void page_zero(unsigned long *page)
{
    unsigned int i;

    for (i = 0; i < PTE_ENTRIES; i++)
        page[i] = 0;
}

static unsigned long prot_to_pte(unsigned long prot)
{
    unsigned long pte = PTE_VALID | PTE_AF;

    if ((prot & MAP_PROT_WRITE))
        pte |= PTE_AP_RW_EL0;
    else
        pte |= PTE_AP_RO_EL0;

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
            return 0;
        return (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    }

    table = alloc_pages(0);
    if (!table)
        return 0;

    page_zero(table);
    parent[index] = __virt_to_phys((unsigned long)table) | PTE_TABLE;
    return table;
}

static int ensure_user_pgtable(void)
{
    if (user_l1)
        return 0;

    user_l1 = alloc_pages(0);
    if (!user_l1)
        return -12;

    page_zero(user_l1);
    return 0;
}

static void install_user_pgtable(void)
{
    unsigned long ttbr0 = __virt_to_phys((unsigned long)user_l1);

    __asm__ volatile(
        "msr ttbr0_el1, %0\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        :
        : "r"(ttbr0));
}

static int map_page(unsigned long va, unsigned long pa, unsigned long prot)
{
    unsigned long *l2;
    unsigned long *l3;
    unsigned long l1_idx = (va >> 30) & 0x1ffUL;
    unsigned long l2_idx = (va >> 21) & 0x1ffUL;
    unsigned long l3_idx = (va >> 12) & 0x1ffUL;

    l2 = get_or_create_table(user_l1, l1_idx);
    if (!l2)
        return -12;

    l3 = get_or_create_table(l2, l2_idx);
    if (!l3)
        return -12;

    l3[l3_idx] = (pa & PTE_ADDR_MASK) | prot_to_pte(prot);
    return 0;
}

int do_map(unsigned long virt, unsigned long phys, unsigned long size,
           unsigned long prot)
{
    unsigned long va;
    unsigned long end;
    unsigned long pte_prot = prot;
    int err;

    if (size == 0)
        return 0;

    err = ensure_user_pgtable();
    if (err)
        return err;

    va = virt & ~(PAGE_SIZE - 1UL);
    end = (virt + size + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    for (; va < end; va += PAGE_SIZE, phys += PAGE_SIZE) {
        err = map_page(va, phys, pte_prot);
        if (err)
            return err;
    }

    install_user_pgtable();
    return 0;
}

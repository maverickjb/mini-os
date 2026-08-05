/*
 * Runtime user mappings via TTBR0 (4 KiB page tables).
 */

#include <linux/mm.h>
#include <asm/memory.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>

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

unsigned long *dup_pgtable(unsigned long *src, int level)
{
    unsigned long *dst;
    unsigned int i;

    dst = alloc_pages(0);
    if (!dst)
        return NULL;

    page_zero(dst);

    for (i = 0; i < PTE_ENTRIES; i++) {
        unsigned long ent = src[i];

        if (!(ent & 1UL))
            continue;

        if (level < 3) {
            unsigned long *sub;
            unsigned long *child_src =
                (unsigned long *)__phys_to_virt(ent & PTE_ADDR_MASK);

            sub = dup_pgtable(child_src, level + 1);
            if (!sub) {
                free_pages(dst, 0);
                return NULL;
            }
            dst[i] = __virt_to_phys((unsigned long)sub) | PTE_TABLE;
        } else {
            dst[i] = ent;
        }
    }

    return dst;
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

static int va_mapped(struct mm_struct *mm, unsigned long va)
{
    unsigned long *l2;
    unsigned long *l3;
    unsigned long l1_idx = (va >> 30) & 0x1ffUL;
    unsigned long l2_idx = (va >> 21) & 0x1ffUL;
    unsigned long l3_idx = (va >> 12) & 0x1ffUL;
    unsigned long entry;

    entry = mm->pgd[l1_idx];
    if (!(entry & 1UL) || (entry & 3UL) != PTE_TABLE)
        return 0;

    l2 = (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    entry = l2[l2_idx];
    if (!(entry & 1UL) || (entry & 3UL) != PTE_TABLE)
        return 0;

    l3 = (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    return (l3[l3_idx] & 1UL) != 0;
}

long do_brk(struct mm_struct *mm, unsigned long newbrk)
{
    unsigned long oldbrk;
    unsigned long addr;
    unsigned long end;
    unsigned long stack_limit;

    if (!mm || !mm->pgd)
        return -EINVAL;

    oldbrk = mm->brk;
    stack_limit = mm->stack_top ? (mm->stack_top - PAGE_SIZE) : 0;

    /* Query / invalid request: return current break (Linux-compatible). */
    if (newbrk < mm->start_brk || (stack_limit && newbrk >= stack_limit))
        return (long)oldbrk;

    if (newbrk == oldbrk)
        return (long)oldbrk;

    if (newbrk > oldbrk) {
        addr = (oldbrk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
        end = (newbrk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

        for (; addr < end; addr += PAGE_SIZE) {
            void *page;
            unsigned long phys;
            int err;

            if (va_mapped(mm, addr))
                continue;

            page = alloc_pages(0);
            if (!page)
                return (long)oldbrk;

            page_zero((unsigned long *)page);
            phys = __virt_to_phys((unsigned long)page);
            err = do_map(mm, addr, phys, PAGE_SIZE,
                         MAP_PROT_READ | MAP_PROT_WRITE);
            if (err) {
                free_pages(page, 0);
                return (long)oldbrk;
            }
        }
    }

    mm->brk = newbrk;
    return (long)newbrk;
}

long ksys_brk(unsigned long brk)
{
    if (!current || !current->is_user || !current->mm)
        return -EINVAL;

    return do_brk(current->mm, brk);
}

static unsigned long find_free_vma(struct mm_struct *mm, unsigned long len)
{
    unsigned long va;
    unsigned long end;
    unsigned long stack_limit;
    unsigned long a;

    stack_limit = mm->stack_top ? (mm->stack_top - PAGE_SIZE) : USER_STACK_TOP;
    va = (mm->mmap_base + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    while (va + len <= stack_limit) {
        end = va + len;
        for (a = va; a < end; a += PAGE_SIZE) {
            if (va_mapped(mm, a))
                break;
        }
        if (a >= end) {
            mm->mmap_base = end;
            return va;
        }
        va = a + PAGE_SIZE;
    }

    return 0;
}

long do_mmap(struct mm_struct *mm, unsigned long addr, unsigned long len,
             unsigned long prot, unsigned long flags)
{
    unsigned long map_prot;
    unsigned long va;
    unsigned long end;
    unsigned long stack_limit;

    if (!mm || !mm->pgd || len == 0)
        return -EINVAL;

    if (!(flags & MAP_ANONYMOUS))
        return -EINVAL;

    len = (len + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
    stack_limit = mm->stack_top ? (mm->stack_top - PAGE_SIZE) : USER_STACK_TOP;

    map_prot = 0;
    if (prot & PROT_READ)
        map_prot |= MAP_PROT_READ;
    if (prot & PROT_WRITE)
        map_prot |= MAP_PROT_WRITE;
    if (prot & PROT_EXEC)
        map_prot |= MAP_PROT_EXEC;
    if (!map_prot)
        return -EINVAL;

    if (!addr || !(flags & MAP_FIXED)) {
        va = find_free_vma(mm, len);
        if (!va)
            return -ENOMEM;
    } else {
        va = addr & ~(PAGE_SIZE - 1UL);
        if (va + len > stack_limit || va < USER_MMAP_BASE)
            return -EINVAL;
        for (end = va; end < va + len; end += PAGE_SIZE) {
            if (va_mapped(mm, end) && !(flags & MAP_FIXED))
                return -ENOMEM;
        }
    }

    for (end = va; end < va + len; end += PAGE_SIZE) {
        void *page;
        unsigned long phys;
        int err;

        if (va_mapped(mm, end))
            continue;

        page = alloc_pages(0);
        if (!page)
            return -ENOMEM;

        page_zero((unsigned long *)page);
        phys = __virt_to_phys((unsigned long)page);
        err = do_map(mm, end, phys, PAGE_SIZE, map_prot);
        if (err) {
            free_pages(page, 0);
            return err;
        }
    }

    return (long)va;
}

long ksys_mmap(unsigned long addr, unsigned long len, unsigned long prot,
               unsigned long flags, unsigned long fd, unsigned long off)
{
    (void)fd;
    (void)off;

    if (!current || !current->is_user || !current->mm)
        return -EINVAL;

    if (!(flags & MAP_ANONYMOUS) && (long)fd >= 0)
        return -ENODEV;

    return do_mmap(current->mm, addr, len, prot, flags);
}

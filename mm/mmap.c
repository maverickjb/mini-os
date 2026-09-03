/*
 * Runtime user mappings via TTBR0 (4 KiB page tables).
 */

#include <linux/mm.h>
#include <asm/memory.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>

#define PTE_VALID       3UL
#define PTE_TABLE       3UL
#define PTE_BLOCK       (1UL << 0)
#define PTE_AF          (1UL << 10)
#define PTE_SHARED      (3UL << 8)   /* inner shareable */
#define PTE_USER        (1UL << 6)   /* AP[1]: user accessible at EL0 */
#define PTE_RDONLY      (1UL << 7)   /* AP[2]: read-only */
#define PTE_NG          (1UL << 11)  /* non-global: follows TTBR0 */
#define PTE_UXN         (1UL << 54)
#define DCACHE_LINE     64UL
/* Output address field [47:12]; must not include upper attr bits (e.g. UXN). */
#define PTE_ADDR_MASK   0x0000FFFFFFFFF000UL
#define PTE_FLAGS_MASK  (~PTE_ADDR_MASK)

#define PTE_ENTRIES     512

static struct kmem_cache vma_cache;

/* ------------------------------------------------------------------ */
/* VMA list — sorted by vm_start, allocated from SLUB                  */
/* ------------------------------------------------------------------ */

struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
    struct vm_area_struct *vma;

    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        if (vma->vm_end > addr)
            return vma;
    }

    return NULL;
}

static struct vm_area_struct *vma_alloc(unsigned long start, unsigned long end,
                                        unsigned long flags)
{
    struct vm_area_struct *vma;

    vma = kmem_cache_alloc(&vma_cache);
    if (!vma)
        return NULL;

    vma->vm_start = start;
    vma->vm_end = end;
    vma->vm_flags = flags;
    vma->vm_next = NULL;
    return vma;
}

static int insert_vma(struct mm_struct *mm, struct vm_area_struct *new)
{
    struct vm_area_struct **pp;
    struct vm_area_struct *prev;

    if (new->vm_start >= new->vm_end)
        return -EINVAL;

    pp = &mm->mmap;
    while (*pp && (*pp)->vm_start < new->vm_start)
        pp = &(*pp)->vm_next;

    if (*pp && new->vm_end > (*pp)->vm_start)
        return -EINVAL;

    if (pp != &mm->mmap) {
        prev = container_of(pp, struct vm_area_struct, vm_next);
        if (prev->vm_end > new->vm_start)
            return -EINVAL;
    }

    new->vm_next = *pp;
    *pp = new;
    return 0;
}

static int remove_vma(struct mm_struct *mm, struct vm_area_struct *vma)
{
    struct vm_area_struct **pp;

    pp = &mm->mmap;
    while (*pp) {
        if (*pp == vma) {
            *pp = vma->vm_next;
            vma->vm_next = NULL;
            return 0;
        }
        pp = &(*pp)->vm_next;
    }

    return -ENOENT;
}

int vma_record(struct mm_struct *mm, unsigned long start, unsigned long end,
               unsigned long vm_flags)
{
    struct vm_area_struct *vma;
    int ret;

    if (!mm || start >= end)
        return -EINVAL;

    vma = vma_alloc(start, end, vm_flags);
    if (!vma)
        return -ENOMEM;

    ret = insert_vma(mm, vma);
    if (ret < 0) {
        kmem_cache_free(&vma_cache, vma);
        return ret;
    }

    return 0;
}

static void free_vma_chain(struct vm_area_struct *vma)
{
    while (vma) {
        struct vm_area_struct *next = vma->vm_next;

        kmem_cache_free(&vma_cache, vma);
        vma = next;
    }
}

struct vm_area_struct *dup_vma_list(struct vm_area_struct *src)
{
    struct vm_area_struct *head = NULL;
    struct vm_area_struct **tail = &head;

    for (; src; src = src->vm_next) {
        struct vm_area_struct *copy = vma_alloc(src->vm_start, src->vm_end,
                                                src->vm_flags);

        if (!copy) {
            free_vma_chain(head);
            return NULL;
        }

        *tail = copy;
        tail = &copy->vm_next;
    }

    return head;
}

void free_all_vmas(struct mm_struct *mm)
{
    struct vm_area_struct *vma;

    if (!mm)
        return;

    while (mm->mmap) {
        vma = mm->mmap;
        mm->mmap = vma->vm_next;
        kmem_cache_free(&vma_cache, vma);
    }
}

static int vma_erase_range(struct mm_struct *mm, unsigned long start,
    unsigned long end)
{
	struct vm_area_struct *vma;

	vma = mm->mmap;
	while (vma) {
		struct vm_area_struct *next = vma->vm_next;
		unsigned long vstart = vma->vm_start;
		unsigned long vend = vma->vm_end;

		if (vend <= start || vstart >= end) {
			vma = next;
			continue;
		}

		if (start <= vstart && end >= vend) {
			remove_vma(mm, vma);
			kmem_cache_free(&vma_cache, vma);
			vma = next;
			continue;
		}

		if (start <= vstart && end < vend) {
			vma->vm_start = end;
			vma = next;
			continue;
		}

		if (start > vstart && end >= vend) {
			vma->vm_end = start;
			vma = next;
			continue;
		}

		if (start > vstart && end < vend) {
			struct vm_area_struct *tail;

			tail = vma_alloc(end, vend, vma->vm_flags);
			if (!tail)
				return -ENOMEM;

			vma->vm_end = start;
			tail->vm_next = vma->vm_next;
			vma->vm_next = tail;
		}

		vma = next;
	}

	return 0;
}

static unsigned long find_unmapped_area(struct mm_struct *mm, unsigned long len)
{
    unsigned long last;
    unsigned long stack_limit;
    struct vm_area_struct *vma;

    stack_limit = mm->stack_top ? (mm->stack_top - USER_STACK_SIZE)
                                : USER_STACK_BOTTOM;
    last = (mm->mmap_base + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        if (last + len <= vma->vm_start && last + len <= stack_limit)
            return last;
        if (vma->vm_end > last)
            last = (vma->vm_end + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
    }

    if (last + len <= stack_limit)
        return last;

    return 0;
}

static struct vm_area_struct *find_heap_vma(struct mm_struct *mm)
{
    struct vm_area_struct *vma;

    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        if (vma->vm_start == mm->start_brk)
            return vma;
    }

    return NULL;
}

static void page_zero(unsigned long *page)
{
    unsigned int i;

    for (i = 0; i < PTE_ENTRIES; i++)
        page[i] = 0;
}

static unsigned long linux_prot_to_map(unsigned long prot)
{
    unsigned long map_prot = 0;

    if (prot & PROT_READ)
        map_prot |= MAP_PROT_READ;
    if (prot & PROT_WRITE)
        map_prot |= MAP_PROT_WRITE;
    if (prot & PROT_EXEC)
        map_prot |= MAP_PROT_EXEC;
    return map_prot;
}

static unsigned long prot_to_pte(unsigned long prot)
{
    unsigned long pte = PTE_VALID | PTE_AF | PTE_SHARED | PTE_NG;

    /*
     * PROT_NONE: keep a valid leaf so the VMA exists, but omit PTE_USER so
     * EL0 faults. Matches Linux: mapping present, no access.
     */
    if (prot & (MAP_PROT_READ | MAP_PROT_WRITE | MAP_PROT_EXEC))
        pte |= PTE_USER;

    if (!(prot & MAP_PROT_WRITE))
        pte |= PTE_RDONLY;

    if (!(prot & MAP_PROT_EXEC))
        pte |= PTE_UXN;

    return pte;
}

static void dcache_clean_poc(void *addr, unsigned long size)
{
    unsigned long p = (unsigned long)addr & ~(DCACHE_LINE - 1UL);
    unsigned long end = (unsigned long)addr + size;

    for (; p < end; p += DCACHE_LINE)
        __asm__ volatile("dc civac, %0" : : "r"(p) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
}

static void pte_set(unsigned long *slot, unsigned long val)
{
    *slot = val;
    __asm__ volatile("dc civac, %0" : : "r"(slot) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
}

static void tlb_flush_all(void)
{
    __asm__ volatile(
        "dsb sy\n"
        "tlbi vmalle1is\n"
        "dsb sy\n"
        "isb\n"
        ::: "memory");
}

static void tlb_flush_page(unsigned long va)
{
    __asm__ volatile(
        "dsb sy\n"
        "tlbi vae1, %0\n"
        "dsb sy\n"
        "isb\n"
        :
        : "r"(va >> 12)
        : "memory");
}

static unsigned long *l3_slot(struct mm_struct *mm, unsigned long va)
{
    unsigned long *l2;
    unsigned long *l3;
    unsigned long l1_idx = (va >> 30) & 0x1ffUL;
    unsigned long l2_idx = (va >> 21) & 0x1ffUL;
    unsigned long l3_idx = (va >> 12) & 0x1ffUL;
    unsigned long entry;

    if (!mm || !mm->pgd)
        return NULL;

    entry = mm->pgd[l1_idx];
    if (!(entry & 1UL) || (entry & 3UL) != PTE_TABLE)
        return NULL;

    l2 = (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    entry = l2[l2_idx];
    if (!(entry & 1UL) || (entry & 3UL) != PTE_TABLE)
        return NULL;

    l3 = (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    return &l3[l3_idx];
}

static int set_page_prot(struct mm_struct *mm, unsigned long va,
                         unsigned long prot)
{
    unsigned long *ptep = l3_slot(mm, va);

    if (!ptep || !(*ptep & 1UL))
        return -ENOMEM;

    pte_set(ptep, (*ptep & PTE_ADDR_MASK) | prot_to_pte(prot));
    tlb_flush_page(va);
    return 0;
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
    dcache_clean_poc(table, PAGE_SIZE);
    pte_set(&parent[index], __virt_to_phys((unsigned long)table) | PTE_TABLE);
    return table;
}

static void free_pgtable_level(unsigned long *table, int level)
{
    unsigned int i;

    if (!table)
        return;

    for (i = 0; i < PTE_ENTRIES; i++) {
        unsigned long ent = table[i];

        if (!(ent & 1UL))
            continue;

        if (level < 3) {
            unsigned long *child =
                (unsigned long *)__phys_to_virt(ent & PTE_ADDR_MASK);

            free_pgtable_level(child, level + 1);
        } else {
            void *page = __phys_to_virt(ent & PTE_ADDR_MASK);

            free_pages(page, 0);
        }
    }

    free_pages(table, 0);
}

/*
 * Free an entire user page-table tree rooted at pgd (level 1),
 * including intermediate tables and mapped leaf pages.
 */
void free_user_page_tables(unsigned long *pgd)
{
    free_pgtable_level(pgd, 1);
}

void mm_put(struct mm_struct *mm)
{
    if (!mm)
        return;

    mm->users--;
    if (mm->users > 0)
        return;

    if (mm->pgd) {
        free_user_page_tables(mm->pgd);
        mm->pgd = NULL;
    }
    free_all_vmas(mm);
    kfree(mm);
}

#define PT_ENTRIES 512

static unsigned long *dup_pgtable_level(unsigned long *src, int level)
{
    unsigned long *dst;
    unsigned int i;

    dst = alloc_pages(0);
    if (!dst)
        return NULL;

    page_zero(dst);
    dcache_clean_poc(dst, PAGE_SIZE);

    for (i = 0; i < PT_ENTRIES; i++) {

        unsigned long ent = src[i];

        /* invalid entry */
        if (!(ent & 1UL))
            continue;


        /*
         * Levels 0-2 contain pointers to lower page tables
         */
        if (level < 3) {

            unsigned long *child_src;
            unsigned long *child_dst;

            child_src =
                (unsigned long *)__phys_to_virt(
                    ent & PTE_ADDR_MASK);

            /*
             * Allocate and copy lower-level table
             */
            child_dst =
                dup_pgtable_level(child_src, level + 1);

            if (!child_dst) {
                free_pages(dst, 0);
                return NULL;
            }


            /*
             * Put new child table address
             */
            pte_set(&dst[i],
                    __virt_to_phys((unsigned long)child_dst) |
                    (ent & PTE_FLAGS_MASK));

        } else {

            /*
             * Level 3: actual page mapping
             *
             * Parent:
             *
             *   VA ---> PA A
             *
             * Child:
             *
             *   VA ---> PA B
             *
             */

            unsigned long old_pa;
            unsigned long new_pa;

            void *old_page;
            void *new_page;


            old_pa = ent & PTE_ADDR_MASK;

            old_page = __phys_to_virt(old_pa);


            /*
             * Allocate child's physical page
             */
            new_page = alloc_pages(0);

            if (!new_page) {
                free_pages(dst, 0);
                return NULL;
            }

            /*
             * Copy user memory
             */
            memcpy(new_page, old_page, PAGE_SIZE);
            dcache_clean_poc(new_page, PAGE_SIZE);

            new_pa =
                __virt_to_phys((unsigned long)new_page);

            pte_set(&dst[i], new_pa | (ent & PTE_FLAGS_MASK));
        }
    }

    return dst;
}

/*
 * Duplicate a user page-table tree. level=1 is the PGD (L1).
 * Intermediate tables are cloned; leaf pages get private physical copies
 * so parent and child do not share mm_struct or mapped pages.
 */
unsigned long *dup_pgtable(unsigned long *src, int level)
{
    return dup_pgtable_level(src, level);
}

void mm_install(struct mm_struct *mm)
{
    unsigned long ttbr0;

    if (!mm || !mm->pgd)
        return;

    ttbr0 = __virt_to_phys((unsigned long)mm->pgd);

    __asm__ volatile(
        "msr ttbr0_el1, %0\n"
        "isb\n"
        :
        : "r"(ttbr0));
    tlb_flush_all();
    /* Drop any stale I-cache lines for previously executed user VAs. */
    __asm__ volatile("ic ialluis\n dsb ish\n isb" ::: "memory");
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

    pte_set(&l3[l3_idx], (pa & PTE_ADDR_MASK) | prot_to_pte(prot));
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

    __asm__ volatile("dsb sy" ::: "memory");
    tlb_flush_all();
    return 0;
}

static int va_mapped(struct mm_struct *mm, unsigned long va)
{
    unsigned long *ptep = l3_slot(mm, va);

    return ptep && (*ptep & 1UL);
}

static int unmap_page(struct mm_struct *mm, unsigned long va)
{
    unsigned long *l2;
    unsigned long *l3;
    unsigned long l1_idx = (va >> 30) & 0x1ffUL;
    unsigned long l2_idx = (va >> 21) & 0x1ffUL;
    unsigned long l3_idx = (va >> 12) & 0x1ffUL;
    unsigned long entry;
    void *page;

    entry = mm->pgd[l1_idx];
    if (!(entry & 1UL) || (entry & 3UL) != PTE_TABLE)
        return 0;

    l2 = (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    entry = l2[l2_idx];
    if (!(entry & 1UL) || (entry & 3UL) != PTE_TABLE)
        return 0;

    l3 = (unsigned long *)__phys_to_virt(entry & PTE_ADDR_MASK);
    entry = l3[l3_idx];
    if (!(entry & 1UL))
        return 0;

    page = __phys_to_virt(entry & PTE_ADDR_MASK);
    pte_set(&l3[l3_idx], 0);
    free_pages(page, 0);
    tlb_flush_page(va);

    return 0;
}

long do_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len)
{
    unsigned long va;
    unsigned long end;
    int err;

    if (!mm || !mm->pgd)
        return -EINVAL;

    if (len == 0)
        return 0;

    if (addr & (PAGE_SIZE - 1UL))
        return -EINVAL;

    end = (addr + len + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    err = vma_erase_range(mm, addr, end);
    if (err)
        return err;

    for (va = addr; va < end; va += PAGE_SIZE)
        unmap_page(mm, va);

    return 0;
}

long do_brk(struct mm_struct *mm, unsigned long newbrk)
{
    unsigned long oldbrk;
    unsigned long addr;
    unsigned long end;
    unsigned long stack_limit;
    struct vm_area_struct *heap_vma;

    if (!mm || !mm->pgd)
        return -EINVAL;

    oldbrk = mm->brk;
    stack_limit = mm->stack_top ? (mm->stack_top - USER_STACK_SIZE) : 0;

    /* Query / invalid request: return current break (Linux-compatible). */
    if (newbrk < mm->start_brk || (stack_limit && newbrk >= stack_limit))
        return (long)oldbrk;

    if (newbrk == oldbrk)
        return (long)oldbrk;

    heap_vma = find_heap_vma(mm);

    if (newbrk > oldbrk) {
        unsigned long new_end;

        new_end = (newbrk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

        if (heap_vma && heap_vma->vm_next &&
            new_end > heap_vma->vm_next->vm_start)
            return (long)oldbrk;

        if (!heap_vma) {
            if (vma_record(mm, mm->start_brk, new_end,
                           MAP_PROT_READ | MAP_PROT_WRITE) < 0)
                return (long)oldbrk;
            heap_vma = find_heap_vma(mm);
        } else if (new_end > heap_vma->vm_end) {
            heap_vma->vm_end = new_end;
        }

        addr = (oldbrk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
        end = new_end;

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
            err = map_page(mm, addr, phys,
                           MAP_PROT_READ | MAP_PROT_WRITE);
            if (err) {
                free_pages(page, 0);
                return (long)oldbrk;
            }
        }
    } else {
        unsigned long old_end;
        unsigned long new_end;

        old_end = (oldbrk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
        new_end = (newbrk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

        for (addr = new_end; addr < old_end; addr += PAGE_SIZE)
            unmap_page(mm, addr);

        if (heap_vma) {
            if (new_end <= heap_vma->vm_start) {
                remove_vma(mm, heap_vma);
                kmem_cache_free(&vma_cache, heap_vma);
            } else {
                heap_vma->vm_end = new_end;
            }
        }
    }

    mm->brk = newbrk;
    __asm__ volatile("dsb sy" ::: "memory");
    tlb_flush_all();
    return (long)newbrk;
}

long ksys_brk(unsigned long brk)
{
    if (!current || !current->is_user || !current->mm)
        return -EINVAL;

    return do_brk(current->mm, brk);
}

long do_mmap(struct mm_struct *mm, unsigned long addr, unsigned long len,
             unsigned long prot, unsigned long flags)
{
    unsigned long map_prot;
    unsigned long va;
    unsigned long end;
    unsigned long stack_limit;
    struct vm_area_struct *vma;
    int err;

    if (!mm || !mm->pgd || len == 0)
        return -EINVAL;

    if (!(flags & MAP_ANONYMOUS))
        return -EINVAL;

    len = (len + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
    stack_limit = mm->stack_top ? (mm->stack_top - USER_STACK_SIZE) : USER_STACK_BOTTOM;
    map_prot = linux_prot_to_map(prot);

    if (!addr || !(flags & MAP_FIXED)) {
        va = find_unmapped_area(mm, len);
        if (!va)
            return -ENOMEM;
    } else {
        va = addr & ~(PAGE_SIZE - 1UL);
        if (!va || va + len > stack_limit)
            return -EINVAL;

        err = vma_erase_range(mm, va, va + len);
        if (err)
            return err;
    }

    vma = vma_alloc(va, va + len, map_prot);
    if (!vma)
        return -ENOMEM;

    err = insert_vma(mm, vma);
    if (err) {
        kmem_cache_free(&vma_cache, vma);
        return err;
    }

    for (end = va; end < va + len; end += PAGE_SIZE) {
        void *page;
        unsigned long phys;

        if (va_mapped(mm, end)) {
            if (flags & MAP_FIXED) {
                err = set_page_prot(mm, end, map_prot);
                if (err)
                    return err;
            }
            continue;
        }

        page = alloc_pages(0);
        if (!page)
            return -ENOMEM;

        page_zero((unsigned long *)page);
        phys = __virt_to_phys((unsigned long)page);
        err = map_page(mm, end, phys, map_prot);
        if (err) {
            free_pages(page, 0);
            return err;
        }
    }

    __asm__ volatile("dsb sy" ::: "memory");
    tlb_flush_all();

    if (va + len > mm->mmap_base)
        mm->mmap_base = va + len;

    return (long)va;
}

void mmap_init(void)
{
    kmem_cache_init(&vma_cache, sizeof(struct vm_area_struct));
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

long ksys_munmap(unsigned long addr, unsigned long len)
{
    if (!current || !current->is_user || !current->mm)
        return -EINVAL;

    return do_munmap(current->mm, addr, len);
}

long ksys_mprotect(unsigned long addr, unsigned long len, unsigned long prot)
{
    unsigned long map_prot;
    unsigned long va;
    unsigned long end;
    int err;

    if (!current || !current->is_user || !current->mm)
        return -EINVAL;

    if (len == 0)
        return 0;

    if (addr & (PAGE_SIZE - 1UL))
        return -EINVAL;

    map_prot = linux_prot_to_map(prot);
    end = (addr + len + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    for (va = addr; va < end; va += PAGE_SIZE) {
        err = set_page_prot(current->mm, va, map_prot);
        if (err)
            return err;
    }

    return 0;
}

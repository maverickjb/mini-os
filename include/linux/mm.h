#ifndef __LINUX_MM_H
#define __LINUX_MM_H

#include <linux/mm_types.h>
#include <asm/ptrace.h>

/* Internal page-table protection bits (also match Linux PROT_*). */
#define MAP_PROT_READ   (1UL << 0)
#define MAP_PROT_WRITE  (1UL << 1)
#define MAP_PROT_EXEC   (1UL << 2)

/* Linux mmap prot / flags (user ABI). */
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
#define PROT_NONE       0x0

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20

/* Anonymous mmap region below the user stack. */
#define USER_MMAP_BASE  0x2000000UL
#define USER_STACK_TOP  0x4040000UL
#define USER_STACK_SIZE 0x10000UL /* 16 pages */
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

#define VM_READ     MAP_PROT_READ
#define VM_WRITE    MAP_PROT_WRITE
#define VM_EXEC     MAP_PROT_EXEC
#define VM_STACK    (1UL << 3)

struct mm_struct *mm_alloc(void);
struct mm_struct *dup_mm(struct mm_struct *oldmm);
unsigned long *dup_pgtable(unsigned long *src, int level);
void mm_put(struct mm_struct *mm);
void free_user_page_tables(unsigned long *pgd);
void mm_install(struct mm_struct *mm);
void mmap_init(void);

int do_map(struct mm_struct *mm, unsigned long virt, unsigned long phys,
           unsigned long size, unsigned long prot);
long do_brk(struct mm_struct *mm, unsigned long newbrk);
long do_mmap(struct mm_struct *mm, unsigned long addr, unsigned long len,
             unsigned long prot, unsigned long flags);
long do_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len);

struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr);
struct vm_area_struct *dup_vma_list(struct vm_area_struct *src);
void free_all_vmas(struct mm_struct *mm);
int vma_record(struct mm_struct *mm, unsigned long start, unsigned long end,
               unsigned long vm_flags);

int do_page_fault(struct mm_struct *mm, unsigned long addr, unsigned long esr);
void do_data_abort(struct pt_regs *regs);

#endif /* __LINUX_MM_H */

/*
 * ELF64 loader for AArch64 executables in ramfs.
 */

#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/errno.h>
#include <asm/memory.h>
#include <asm/ptrace.h>

#include "binfmt.h"
#include <linux/mm.h>
#include <linux/gfp.h>

#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5

#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define ET_EXEC         2
#define EM_AARCH64      183
#define PT_LOAD         1

#define PF_X            1
#define PF_W            2
#define PF_R            4

typedef struct {
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
    unsigned long e_entry;
    unsigned long e_phoff;
    unsigned long e_shoff;
    unsigned int e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    unsigned int p_type;
    unsigned int p_flags;
    unsigned long p_offset;
    unsigned long p_vaddr;
    unsigned long p_paddr;
    unsigned long p_filesz;
    unsigned long p_memsz;
    unsigned long p_align;
} Elf64_Phdr;

extern void task_trampoline(void);

static void kmemcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
}

static void kmemset(void *dst, int c, unsigned long n)
{
    unsigned char *d = dst;

    while (n--)
        *d++ = (unsigned char)c;
}

static unsigned long elf_prot(unsigned long p_flags)
{
    unsigned long prot = 0;

    if (p_flags & PF_R)
        prot |= MAP_PROT_READ;
    if (p_flags & PF_W)
        prot |= MAP_PROT_WRITE;
    if (p_flags & PF_X)
        prot |= MAP_PROT_EXEC;

    return prot;
}

static int setup_stack(struct mm_struct *mm, unsigned long *stack_top_out)
{
    void *stack = alloc_pages(0);
    unsigned long stack_phys;
    unsigned long map_base;
    int err;

    if (!stack)
        return -ENOMEM;

    stack_phys = __virt_to_phys((unsigned long)stack);
    map_base = (USER_STACK_TOP - PAGE_SIZE) & ~(PAGE_SIZE - 1UL);

    err = do_map(mm, map_base, stack_phys, PAGE_SIZE,
                 MAP_PROT_READ | MAP_PROT_WRITE);
    if (err)
        return err;

    *stack_top_out = map_base + PAGE_SIZE;
    return 0;
}

static int load_segment(struct mm_struct *mm, const unsigned char *buf,
                        unsigned long len, const Elf64_Phdr *ph)
{
    unsigned long vaddr = ph->p_vaddr;
    unsigned long memsz = ph->p_memsz;
    unsigned long filesz = ph->p_filesz;
    unsigned long offset = ph->p_offset;
    unsigned long map_start = vaddr & ~(PAGE_SIZE - 1UL);
    unsigned long map_end = (vaddr + memsz + PAGE_SIZE - 1UL) &
                            ~(PAGE_SIZE - 1UL);
    unsigned long va;
    unsigned long file_pos = 0;

    /* BSS-only segments (filesz == 0) may have p_offset past EOF. */
    if (filesz && offset + filesz > len)
        return -ENOEXEC;

    for (va = map_start; va < map_end; va += PAGE_SIZE) {
        void *page;
        unsigned long phys;
        unsigned long page_skip;
        unsigned long copy_len;
        int err;

        page = alloc_pages(0);
        if (!page)
            return -ENOMEM;

        kmemset(page, 0, PAGE_SIZE);
        phys = __virt_to_phys((unsigned long)page);

        err = do_map(mm, va, phys, PAGE_SIZE, elf_prot(ph->p_flags));
        if (err) {
            free_pages(page, 0);
            return err;
        }

        page_skip = (va == map_start) ? (vaddr - map_start) : 0;

        if (file_pos < filesz) {
            unsigned long file_left = filesz - file_pos;
            unsigned long room = PAGE_SIZE - page_skip;

            copy_len = file_left < room ? file_left : room;
            kmemcpy((unsigned char *)page + page_skip,
                    buf + offset + file_pos, copy_len);
            file_pos += copy_len;
        }
    }

    return 0;
}

int load_elf_binary(struct linux_binprm *bprm)
{
    const Elf64_Ehdr *ehdr;
    const Elf64_Phdr *phdrs;
    struct mm_struct *mm;
    struct mm_struct *old_mm;
    unsigned int i;
    int err;

    if (!bprm || !bprm->buf || bprm->len < sizeof(Elf64_Ehdr))
        return -ENOEXEC;

    if (!bprm->task)
        return -EINVAL;

    ehdr = (const Elf64_Ehdr *)bprm->buf;

    if (ehdr->e_ident[EI_MAG0] != 0x7f || ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' || ehdr->e_ident[EI_MAG3] != 'F')
        return -ENOEXEC;

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
        return -ENOEXEC;

    if (ehdr->e_type != ET_EXEC || ehdr->e_machine != EM_AARCH64)
        return -ENOEXEC;

    if (ehdr->e_phoff + (unsigned long)ehdr->e_phnum * sizeof(Elf64_Phdr) >
        bprm->len)
        return -ENOEXEC;

    mm = mm_alloc();
    if (!mm)
        return -ENOMEM;

    phdrs = (const Elf64_Phdr *)(bprm->buf + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *ph = &phdrs[i];
        unsigned long seg_end;

        if (ph->p_type != PT_LOAD)
            continue;

        err = load_segment(mm, bprm->buf, bprm->len, ph);
        if (err) {
            mm_put(mm);
            return err;
        }

        seg_end = ph->p_vaddr + ph->p_memsz;
        if (seg_end > mm->start_brk)
            mm->start_brk = seg_end;
    }

    mm->start_brk = (mm->start_brk + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);
    mm->brk = mm->start_brk;

    err = setup_stack(mm, &bprm->stack_top);
    if (err) {
        mm_put(mm);
        return err;
    }

    mm->entry = ehdr->e_entry;
    mm->stack_top = bprm->stack_top;
    bprm->entry = mm->entry;

    old_mm = bprm->task->mm;
    bprm->task->mm = mm;
    if (old_mm)
        mm_put(old_mm);

    bprm->task->is_user = 1;
    bprm->task->user_sp = mm->stack_top;
    {
        struct pt_regs *regs = (struct pt_regs *)bprm->task->stack;

        kmemset(regs, 0, sizeof(*regs));
        regs->elr_el1 = mm->entry;
        regs->spsr_el1 = 0;
        bprm->task->regs = regs;
    }

    task_user_ctx_init(bprm->task);
    ret_from_fork();

    return 0;
}
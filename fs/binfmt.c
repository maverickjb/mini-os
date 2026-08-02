/*
 * ELF64 loader for AArch64 executables in ramfs.
 */

#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/errno.h>

#include "binfmt.h"
#include "mmap.h"
#include "page_alloc.h"
#include "mem.h"

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

#define USER_STACK_TOP  0x4040000UL
#define USER_STACK_SIZE (64UL * 1024UL)

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

static int pages_to_order(unsigned long npages)
{
    int order = 0;
    unsigned long count = 1;

    while (count < npages && order < 15) {
        count <<= 1;
        order++;
    }

    return order;
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
    unsigned long page_off = vaddr & (PAGE_SIZE - 1UL);
    unsigned long map_start = vaddr & ~(PAGE_SIZE - 1UL);
    unsigned long map_end = (vaddr + memsz + PAGE_SIZE - 1UL) &
                            ~(PAGE_SIZE - 1UL);
    unsigned long map_size = map_end - map_start;
    unsigned long npages = map_size / PAGE_SIZE;
    void *mem;
    unsigned long phys;
    int err;

    if (offset + filesz > len)
        return -ENOEXEC;

    mem = alloc_pages(pages_to_order(npages));
    if (!mem)
        return -ENOMEM;

    phys = __virt_to_phys((unsigned long)mem);

    err = do_map(mm, map_start, phys, map_size, elf_prot(ph->p_flags));
    if (err)
        return err;

    if (filesz)
        kmemcpy((unsigned char *)mem + page_off, buf + offset, filesz);

    if (memsz > filesz)
        kmemset((unsigned char *)mem + page_off + filesz, 0, memsz - filesz);

    return 0;
}

int load_elf_binary(struct linux_binprm *bprm)
{
    const Elf64_Ehdr *ehdr;
    const Elf64_Phdr *phdrs;
    struct mm_struct *mm;
    unsigned int i;
    int err;

    if (!bprm || !bprm->buf || bprm->len < sizeof(Elf64_Ehdr))
        return -ENOEXEC;

    if (!bprm->task)
        return -EINVAL;

    mm = mm_alloc();
    if (!mm)
        return -ENOMEM;

    bprm->task->mm = mm;

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

    phdrs = (const Elf64_Phdr *)(bprm->buf + ehdr->e_phoff);

    __asm__ volatile("msr daifset, #3");

    for (i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *ph = &phdrs[i];

        if (ph->p_type != PT_LOAD)
            continue;

        err = load_segment(mm, bprm->buf, bprm->len, ph);
        if (err)
            return err;
    }

    err = setup_stack(mm, &bprm->stack_top);
    if (err)
        return err;

    mm->entry = ehdr->e_entry;
    mm->stack_top = bprm->stack_top;
    bprm->entry = mm->entry;

    bprm->task->is_user = 1;
    bprm->task->user_sp = mm->stack_top;
    kmemset(&bprm->task->user_regs, 0, sizeof(bprm->task->user_regs));
    bprm->task->user_regs.elr_el1 = mm->entry;
    bprm->task->user_regs.spsr_el1 = 0;

    task_user_ctx_init(bprm->task);
    ret_from_fork();
}
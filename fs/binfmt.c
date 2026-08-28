/*
 * ELF64 loader for AArch64 executables in ramfs.
 */

#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/errno.h>
#include <asm/memory.h>
#include <asm/ptrace.h>

#include <linux/binfmts.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/auxvec.h>
#include <linux/string.h>
#include <linux/tick.h>

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

static int setup_stack(struct mm_struct *mm, unsigned long *stack_top_out,
                       void **kpage_out)
{
    void *top_page = NULL;
    unsigned long pages = USER_STACK_SIZE / PAGE_SIZE;
    unsigned long i;

    for (i = 0; i < pages; i++) {
        void *stack = alloc_pages(0);
        unsigned long map_base;
        unsigned long stack_phys;
        int err;

        if (!stack)
            return -ENOMEM;

        kmemset(stack, 0, PAGE_SIZE);
        stack_phys = __virt_to_phys((unsigned long)stack);
        map_base = USER_STACK_TOP - (i + 1UL) * PAGE_SIZE;

        err = do_map(mm, map_base, stack_phys, PAGE_SIZE,
                     MAP_PROT_READ | MAP_PROT_WRITE);
        if (err)
            return err;

        /* create_elf_tables fills argv/env into the topmost page. */
        if (i == 0)
            top_page = stack;
    }

    *kpage_out = top_page;
    *stack_top_out = USER_STACK_TOP;
    return 0;
}

static int stack_put_string(char *kpage, unsigned long map_base,
                            unsigned long *sp, const char *s,
                            unsigned long *uaddr)
{
    unsigned long len = strlen(s) + 1;

    if (*sp < map_base + len)
        return -E2BIG;
    *sp -= len;
    memcpy(kpage + (*sp - map_base), s, len);
    *uaddr = *sp;
    return 0;
}

/*
 * Linux user stack at exec (SP at the low end, stack grows down):
 *
 *   high: strings, AT_RANDOM bytes
 *         auxv … AT_NULL
 *         envp[] NULL
 *         argv[] NULL
 *   SP  : argc
 */
static int create_elf_tables(char *kpage, unsigned long map_base,
                             unsigned long stack_top, struct linux_binprm *bprm,
                             const Elf64_Ehdr *ehdr, const Elf64_Phdr *phdrs,
                             unsigned long *user_sp)
{
    unsigned long sp = stack_top;
    unsigned long argv_u[MAX_EXEC_ARGS];
    unsigned long envp_u[MAX_EXEC_ARGS];
    unsigned long random_u;
    unsigned long execfn_u;
    unsigned long phdr_addr = 0;
    unsigned long *slot;
    unsigned int i;
    unsigned int naux = 17;
    unsigned long table_bytes;
    int err;

    for (i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *ph = &phdrs[i];

        if (ph->p_type != PT_LOAD)
            continue;
        if (ph->p_offset <= ehdr->e_phoff &&
            ehdr->e_phoff < ph->p_offset + ph->p_filesz) {
            phdr_addr = ph->p_vaddr + (ehdr->e_phoff - ph->p_offset);
            break;
        }
    }

    err = stack_put_string(kpage, map_base, &sp,
                           bprm->filename ? bprm->filename : bprm->argv[0],
                           &execfn_u);
    if (err)
        return err;

    for (i = (unsigned int)bprm->envc; i-- > 0; ) {
        err = stack_put_string(kpage, map_base, &sp, bprm->envp[i], &envp_u[i]);
        if (err)
            return err;
    }
    for (i = (unsigned int)bprm->argc; i-- > 0; ) {
        err = stack_put_string(kpage, map_base, &sp, bprm->argv[i], &argv_u[i]);
        if (err)
            return err;
    }

    if (sp < map_base + 16)
        return -E2BIG;
    sp -= 16;
    random_u = sp;
    for (i = 0; i < 16; i++)
        kpage[sp - map_base + i] = (char)(0xa5 ^ (int)i ^ (int)sp);

    table_bytes = (1UL + (unsigned long)bprm->argc + 1UL +
                   (unsigned long)bprm->envc + 1UL +
                   (unsigned long)naux * 2UL) * sizeof(unsigned long);
    if (sp < map_base + table_bytes + 16)
        return -E2BIG;
    sp -= table_bytes;
    sp &= ~15UL;

    slot = (unsigned long *)(kpage + (sp - map_base));
    *slot++ = (unsigned long)bprm->argc;
    for (i = 0; i < (unsigned int)bprm->argc; i++)
        *slot++ = argv_u[i];
    *slot++ = 0;
    for (i = 0; i < (unsigned int)bprm->envc; i++)
        *slot++ = envp_u[i];
    *slot++ = 0;

#define AUX(t, v) do { *slot++ = (unsigned long)(t); *slot++ = (v); } while (0)
    AUX(AT_PHDR, phdr_addr);
    AUX(AT_PHENT, sizeof(Elf64_Phdr));
    AUX(AT_PHNUM, ehdr->e_phnum);
    AUX(AT_PAGESZ, PAGE_SIZE);
    AUX(AT_BASE, 0);
    AUX(AT_FLAGS, 0);
    AUX(AT_ENTRY, ehdr->e_entry);
    AUX(AT_UID, 0);
    AUX(AT_EUID, 0);
    AUX(AT_GID, 0);
    AUX(AT_EGID, 0);
    AUX(AT_HWCAP, 0);
    AUX(AT_CLKTCK, HZ);
    AUX(AT_SECURE, 0);
    AUX(AT_RANDOM, random_u);
    AUX(AT_EXECFN, execfn_u);
    AUX(AT_NULL, 0);
#undef AUX

    *user_sp = sp;
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

        /*
         * Written through the D-cache; EL0 I-fetch uses the I-cache (PoU).
         * Without this, a later exec of the same VAs can run stale lines.
         */
        {
            unsigned long p = (unsigned long)page;
            unsigned long end = p + PAGE_SIZE;

            for (; p < end; p += 64)
                __asm__ volatile("dc cvau, %0" : : "r"(p) : "memory");
            __asm__ volatile("dsb ish" ::: "memory");
            if (ph->p_flags & PF_X) {
                p = (unsigned long)page;
                for (; p < end; p += 64)
                    __asm__ volatile("ic ivau, %0" : : "r"(p) : "memory");
                __asm__ volatile("dsb ish\n isb" ::: "memory");
            }
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
    void *kstack;
    unsigned long user_sp;
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

    err = setup_stack(mm, &bprm->stack_top, &kstack);
    if (err) {
        mm_put(mm);
        return err;
    }

    err = create_elf_tables(kstack, bprm->stack_top - PAGE_SIZE,
                            bprm->stack_top, bprm, ehdr, phdrs, &user_sp);
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
    bprm->task->user_sp = user_sp;
    /* New image installs its own TLS; drop the previous TPIDR_EL0. */
    bprm->task->tpidr_el0 = 0;

    /* /proc/<pid>/stat + cmdline — keep this off the syscall stack in do_execve. */
    {
        const char *arg0 = bprm->argv[0];
        const char *base = arg0;
        const char *p;
        unsigned long pos = 0;
        int ai;

        for (p = arg0; *p; p++)
            if (*p == '/')
                base = p + 1;
        strscpy(bprm->task->comm, base, sizeof(bprm->task->comm));

        memset(bprm->task->cmdline, 0, sizeof(bprm->task->cmdline));
        for (ai = 0; ai < bprm->argc; ai++) {
            const char *s = bprm->argv[ai];

            while (*s && pos + 1 < sizeof(bprm->task->cmdline))
                bprm->task->cmdline[pos++] = *s++;
            if (pos + 1 < sizeof(bprm->task->cmdline))
                bprm->task->cmdline[pos++] = '\0';
            else
                break;
        }
    }

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

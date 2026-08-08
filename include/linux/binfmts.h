#ifndef _LINUX_BINFMTS_H
#define _LINUX_BINFMTS_H

struct task_struct;

struct linux_binprm {
    const unsigned char *buf;
    unsigned long len;
    unsigned long entry;
    unsigned long stack_top;
    struct task_struct *task;
};

int load_elf_binary(struct linux_binprm *bprm);
int kernel_execve(const char *kernel_filename);

#endif /* _LINUX_BINFMTS_H */

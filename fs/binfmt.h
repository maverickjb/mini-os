#ifndef BINFMT_H
#define BINFMT_H

struct task_struct;

struct linux_binprm {
    const unsigned char *buf;
    unsigned long len;
    unsigned long entry;
    unsigned long stack_top;
    struct task_struct *task;
};

int load_elf_binary(struct linux_binprm *bprm);

#endif

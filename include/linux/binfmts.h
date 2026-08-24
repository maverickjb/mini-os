#ifndef _LINUX_BINFMTS_H
#define _LINUX_BINFMTS_H

struct task_struct;

#define MAX_EXEC_ARGS	8
#define MAX_EXEC_ARG_LEN	128

struct linux_binprm {
    const unsigned char *buf;
    const char *filename;
    unsigned long len;
    unsigned long entry;
    unsigned long stack_top;
    struct task_struct *task;
    int argc;
    int envc;
    char argv[MAX_EXEC_ARGS][MAX_EXEC_ARG_LEN];
    char envp[MAX_EXEC_ARGS][MAX_EXEC_ARG_LEN];
};

int load_elf_binary(struct linux_binprm *bprm);
int kernel_execve(const char *kernel_filename);

#endif /* _LINUX_BINFMTS_H */

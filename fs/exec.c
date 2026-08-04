/*
 * kernel_execve / ksys_execve — load and run an ELF from ramfs.
 */

#include <linux/sched/task.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include "exec.h"
#include "binfmt.h"
#include "ramfs.h"

#define PATH_MAX 256

static int copy_path_from_user(char *dst, const char *src, unsigned long max)
{
    unsigned long i;

    if (!src || max == 0)
        return -EINVAL;

    for (i = 0; i < max; i++) {
        if (copy_from_user(dst + i, src + i, 1))
            return -EFAULT;
        if (dst[i] == '\0')
            return 0;
    }

    return -ENAMETOOLONG;
}

static int do_execve(const char *filename)
{
    struct ramfs_inode *inode;
    struct linux_binprm bprm;

    if (!filename || filename[0] == '\0')
        return -EINVAL;

    inode = ramfs_lookup(filename);
    if (!inode)
        return -ENOENT;

    if (!ramfs_is_reg(inode))
        return -ENOEXEC;

    bprm.buf = (const unsigned char *)ramfs_data(inode);
    bprm.len = ramfs_size(inode);
    bprm.entry = 0;
    bprm.stack_top = 0;
    bprm.task = get_current();

    if (!bprm.buf || bprm.len == 0)
        return -ENOEXEC;

    return load_elf_binary(&bprm);
}

int kernel_execve(const char *kernel_filename)
{
    return do_execve(kernel_filename);
}

long ksys_execve(struct pt_regs *regs, const char *filename,
                 char *const *argv, char *const *envp)
{
    char path[PATH_MAX];
    int err;

    (void)regs;
    (void)argv;
    (void)envp;

    if (!current || !current->is_user)
        return -EINVAL;

    err = copy_path_from_user(path, filename, PATH_MAX);
    if (err)
        return err;

    return do_execve(path);
}

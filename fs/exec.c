/*
 * kernel_execve — load and run an ELF from ramfs.
 */

#include <linux/sched/task.h>
#include <linux/errno.h>

#include "exec.h"
#include "binfmt.h"
#include "ramfs.h"

int kernel_execve(const char *kernel_filename)
{
    struct ramfs_inode *inode;
    struct linux_binprm bprm;

    if (!kernel_filename)
        return -EINVAL;

    inode = ramfs_lookup(kernel_filename);
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

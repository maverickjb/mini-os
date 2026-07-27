/*
 * kernel_execve — load and run an ELF from ramfs.
 */

#include "exec.h"
#include "binfmt.h"
#include "ramfs.h"
#include "fork.h"

int kernel_execve(const char *kernel_filename)
{
    struct ramfs_inode *inode;
    struct linux_binprm bprm;

    if (!kernel_filename)
        return -22;

    inode = ramfs_lookup(kernel_filename);
    if (!inode)
        return -2;

    if (!ramfs_is_reg(inode))
        return -8;

    bprm.buf = (const unsigned char *)ramfs_data(inode);
    bprm.len = ramfs_size(inode);
    bprm.entry = 0;
    bprm.stack_top = 0;
    bprm.task = get_current();

    if (!bprm.buf || bprm.len == 0)
        return -8;

    return load_elf_binary(&bprm);
}

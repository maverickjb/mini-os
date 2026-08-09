/*
 * kernel_execve / ksys_execve — load and run an ELF from ramfs.
 */

#include <linux/sched/task.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/fs.h>

#include <linux/binfmts.h>
#include <linux/namei.h>
#include <linux/ramfs.h>

static int do_execve(const char *filename)
{
    struct inode *inode;
    struct linux_binprm bprm;

    if (!filename || filename[0] == '\0')
        return -EINVAL;

    inode = vfs_lookup(filename);
    if (!inode)
        return -ENOENT;

    if (!inode_is_reg(inode))
        return -ENOEXEC;

    bprm.buf = (const unsigned char *)ramfs_data(inode);
    bprm.len = inode->size;
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

    err = getname_from_user(path, filename);
    if (err)
        return err;

    return do_execve(path);
}

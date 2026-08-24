/*
 * kernel_execve / ksys_execve — load and run an ELF from ramfs.
 */

#include <linux/sched/task.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/string.h>

#include <linux/binfmts.h>
#include <linux/namei.h>
#include <linux/ramfs.h>

static int copy_str_list(char *const *list, int from_user,
                         char out[][MAX_EXEC_ARG_LEN], int *count)
{
    int i;

    *count = 0;
    if (!list)
        return 0;

    for (i = 0; i < MAX_EXEC_ARGS; i++) {
        const char *s;
        long n;

        if (from_user) {
            unsigned long ptr;

            if (copy_from_user(&ptr, &list[i], sizeof(ptr)))
                return -EFAULT;
            if (!ptr)
                break;
            n = strncpy_from_user(out[i], (const char *)ptr, MAX_EXEC_ARG_LEN);
            if (n < 0)
                return (int)n;
            if (n == MAX_EXEC_ARG_LEN)
                return -E2BIG;
        } else {
            s = ((const char *const *)list)[i];
            if (!s)
                break;
            n = (long)strscpy(out[i], s, MAX_EXEC_ARG_LEN);
            if (n < 0)
                return -E2BIG;
        }
        (*count)++;
    }

    return 0;
}

static int do_execve(const char *filename, char *const *argv, char *const *envp,
                     int from_user)
{
    struct inode *inode;
    struct linux_binprm bprm;
    int err;

    if (!filename || filename[0] == '\0')
        return -EINVAL;

    inode = vfs_lookup(filename);
    if (!inode)
        return -ENOENT;

    if (!inode_is_reg(inode))
        return -ENOEXEC;

    memset(&bprm, 0, sizeof(bprm));
    bprm.buf = (const unsigned char *)ramfs_data(inode);
    bprm.filename = filename;
    bprm.len = inode->size;
    bprm.task = get_current();

    if (!bprm.buf || bprm.len == 0)
        return -ENOEXEC;

    err = copy_str_list(argv, from_user, bprm.argv, &bprm.argc);
    if (err)
        return err;

    err = copy_str_list(envp, from_user, bprm.envp, &bprm.envc);
    if (err)
        return err;

    if (bprm.argc == 0) {
        if (strscpy(bprm.argv[0], filename, MAX_EXEC_ARG_LEN) < 0)
            return -E2BIG;
        bprm.argc = 1;
    }

    return load_elf_binary(&bprm);
}

int kernel_execve(const char *kernel_filename)
{
    const char *argv[2];

    argv[0] = kernel_filename;
    argv[1] = NULL;
    return do_execve(kernel_filename, (char *const *)argv, NULL, 0);
}

long ksys_execve(struct pt_regs *regs, const char *filename,
                 char *const *argv, char *const *envp)
{
    char path[PATH_MAX];
    int err;

    (void)regs;

    if (!current || !current->is_user)
        return -EINVAL;

    err = getname_from_user(path, filename);
    if (err)
        return err;

    return do_execve(path, argv, envp, 1);
}

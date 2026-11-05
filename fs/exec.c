/*
 * kernel_execve / ksys_execve — load and run an ELF from ramfs.
 */

#include <linux/sched/task.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/binfmts.h>
#include <linux/namei.h>
#include <linux/ramfs.h>

static int copy_one_user_ptr(unsigned long list, unsigned int i,
                             unsigned long *ptr)
{
    unsigned long slot = list + (unsigned long)i * sizeof(unsigned long);

    if (copy_from_user(ptr, (const void *)slot, sizeof(*ptr)))
        return -EFAULT;
    return 0;
}

static int copy_str_list(char *const *list, int from_user,
                         char out[][MAX_EXEC_ARG_LEN], int *count)
{
    unsigned int i;
    unsigned long list_addr = (unsigned long)list;

    *count = 0;
    if (!list)
        return 0;

    for (i = 0; i < MAX_EXEC_ARGS; i++) {
        unsigned long ptr;
        long n;

        if (from_user) {
            n = copy_one_user_ptr(list_addr, i, &ptr);
            if (n)
                return (int)n;
            if (!ptr)
                return 0;
            n = strncpy_from_user(out[i], (const char *)ptr, MAX_EXEC_ARG_LEN);
        } else {
            const char *s = ((const char *const *)list)[i];

            if (!s)
                return 0;
            n = (long)strscpy(out[i], s, MAX_EXEC_ARG_LEN);
        }
        if (n < 0)
            return (n == -E2BIG) ? -E2BIG : (int)n;
        if (n == MAX_EXEC_ARG_LEN)
            return -E2BIG;
        (*count)++;
    }

    /* The list must be NULL-terminated within MAX_EXEC_ARGS+1 slots. */
    if (from_user) {
        unsigned long ptr;
        int err = copy_one_user_ptr(list_addr, MAX_EXEC_ARGS, &ptr);

        if (err)
            return err;
        if (ptr)
            return -E2BIG;
    } else if (((const char *const *)list)[MAX_EXEC_ARGS])
        return -E2BIG;

    return 0;
}

static int do_execve(const char *filename, char *const *argv, char *const *envp,
                     int from_user)
{
    struct inode *inode;
    struct linux_binprm *bprm;
    int err;

    if (!filename || filename[0] == '\0')
        return -EINVAL;

    inode = vfs_lookup(filename);
    if (!inode)
        return -ENOENT;

    if (!inode_is_reg(inode)) {
        iput(inode);
        return -ENOEXEC;
    }

    bprm = kmalloc(sizeof(*bprm));
    if (!bprm) {
        iput(inode);
        return -ENOMEM;
    }
    memset(bprm, 0, sizeof(*bprm));
    bprm->buf = (const unsigned char *)ramfs_data(inode);
    bprm->filename = filename;
    bprm->len = inode->size;
    bprm->task = get_current();

    if (!bprm->buf || bprm->len == 0) {
        kfree(bprm);
        iput(inode);
        return -ENOEXEC;
    }

    err = copy_str_list(argv, from_user, bprm->argv, &bprm->argc);
    if (err) {
        kfree(bprm);
        iput(inode);
        return err;
    }

    err = copy_str_list(envp, from_user, bprm->envp, &bprm->envc);
    if (err) {
        kfree(bprm);
        iput(inode);
        return err;
    }

    if (bprm->argc == 0) {
        if (strscpy(bprm->argv[0], filename, MAX_EXEC_ARG_LEN) < 0) {
            kfree(bprm);
            iput(inode);
            return -E2BIG;
        }
        bprm->argc = 1;
    }

    err = load_elf_binary(bprm);
    /* Success does not return: load_elf_binary frees bprm then eret. */
    kfree(bprm);
    iput(inode);
    return err;
}

int kernel_execve(const char *kernel_filename)
{
    const char *argv[2];
    /* TZ is userspace display policy (POSIX CST-8 = UTC+8). The kernel
     * clock stays UTC; BusyBox init inherits this env for ash/`date`. */
    const char *envp[] = {
        "PATH=/bin:/sbin",
        "HOME=/",
        "TERM=linux",
        "TZ=CST-8",
        NULL,
    };

    argv[0] = kernel_filename;
    argv[1] = NULL;
    return do_execve(kernel_filename, (char *const *)argv,
                     (char *const *)envp, 0);
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

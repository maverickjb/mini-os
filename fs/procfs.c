/*
 * Minimal /proc for BusyBox ps — /proc, /proc/<pid>/, stat, cmdline.
 */

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/ramfs.h>
#include <linux/dirent.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/namei.h>

#define PROC_MAGIC      0x50524f43UL /* 'PROC' */
#define PROC_KIND_DIR   1
#define PROC_KIND_STAT  2
#define PROC_KIND_CMDLINE 3

struct proc_inode {
    struct inode inode;
    unsigned long magic;
    pid_t pid;
    int kind;
    unsigned long len;
    char data[512];
};

static struct inode *proc_root_inode;
static struct file_ops proc_root_ops;
static struct file_ops proc_pid_dir_ops;
static struct file_ops proc_file_ops;

static struct proc_inode *PROC_I(const struct inode *inode)
{
    return container_of(inode, struct proc_inode, inode);
}

int proc_is_inode(const struct inode *inode)
{
    if (!inode || !inode->i_fop)
        return 0;
    return inode->i_fop == &proc_pid_dir_ops ||
           inode->i_fop == &proc_file_ops;
}

void proc_iput(struct inode *inode)
{
    struct proc_inode *pi;

    if (!proc_is_inode(inode))
        return;
    pi = PROC_I(inode);
    if (pi->magic != PROC_MAGIC)
        return;
    pi->magic = 0;
    free_pages(pi, 0);
}

static struct task_struct *proc_find_task(pid_t pid)
{
    struct list_head *pos;
    struct task_struct *walk;
    struct task_struct *found = NULL;
    unsigned long flags;

    if (pid <= 0)
        return NULL;

    task_list_lock_irqsave(&flags);
    for_each_task(pos, walk) {
        if (walk->pid != pid)
            continue;
        if (walk->state == TASK_DEAD)
            continue;
        if (!walk->is_user && walk->state != TASK_ZOMBIE)
            continue;
        found = walk;
        break;
    }
    task_list_unlock_irqrestore(flags);
    return found;
}

static char state_char(enum task_state state)
{
    switch (state) {
    case TASK_RUNNING:
        return 'R';
    case TASK_SLEEPING:
        return 'S';
    case TASK_STOPPED:
        return 'T';
    case TASK_ZOMBIE:
        return 'Z';
    default:
        return 'S';
    }
}

static unsigned long append_char(char *buf, unsigned long size,
                                 unsigned long pos, char c)
{
    if (pos + 1 < size)
        buf[pos] = c;
    return pos + 1;
}

static unsigned long append_str(char *buf, unsigned long size,
                                unsigned long pos, const char *s)
{
    while (*s) {
        pos = append_char(buf, size, pos, *s);
        s++;
    }
    return pos;
}

static unsigned long append_uint(char *buf, unsigned long size,
                                 unsigned long pos, unsigned long v)
{
    char tmp[20];
    int i = 0;

    if (v == 0)
        return append_char(buf, size, pos, '0');

    while (v && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (char)(v % 10);
        v /= 10;
    }
    while (i > 0)
        pos = append_char(buf, size, pos, tmp[--i]);
    return pos;
}

static void fill_stat(struct proc_inode *pi, struct task_struct *task)
{
    unsigned long pos = 0;
    char *buf = pi->data;
    unsigned long size = sizeof(pi->data);
    pid_t ppid = task->parent ? task->parent->pid : 0;
    const char *comm = task->comm[0] ? task->comm : "unknown";
    int i;

    pos = append_uint(buf, size, pos, (unsigned long)task->pid);
    pos = append_char(buf, size, pos, ' ');
    pos = append_char(buf, size, pos, '(');
    pos = append_str(buf, size, pos, comm);
    pos = append_char(buf, size, pos, ')');
    pos = append_char(buf, size, pos, ' ');
    pos = append_char(buf, size, pos, state_char(task->state));
    pos = append_char(buf, size, pos, ' ');
    pos = append_uint(buf, size, pos, (unsigned long)ppid);
    pos = append_char(buf, size, pos, ' ');
    pos = append_uint(buf, size, pos, (unsigned long)task->pgid);
    pos = append_char(buf, size, pos, ' ');
    pos = append_uint(buf, size, pos, (unsigned long)task->sid);

    /* Remaining Linux fields (tty_nr …) — zeros are enough for BusyBox ps. */
    for (i = 0; i < 46; i++) {
        pos = append_char(buf, size, pos, ' ');
        pos = append_char(buf, size, pos, '0');
    }
    pos = append_char(buf, size, pos, '\n');

    if (pos >= size)
        pos = size - 1;
    buf[pos] = '\0';
    pi->len = pos;
    pi->inode.size = pos;
}

static void fill_cmdline(struct proc_inode *pi, struct task_struct *task)
{
    unsigned long i = 0;

    if (!task->cmdline[0]) {
        if (task->comm[0]) {
            while (i + 1 < sizeof(pi->data) && task->comm[i]) {
                pi->data[i] = task->comm[i];
                i++;
            }
            pi->data[i++] = '\0';
        }
        pi->len = i;
        pi->inode.size = i;
        return;
    }

    while (i < sizeof(task->cmdline)) {
        if (task->cmdline[i] == '\0') {
            i++;
            if (i >= sizeof(task->cmdline) || task->cmdline[i] == '\0')
                break;
        } else {
            i++;
        }
    }

    if (i > sizeof(pi->data))
        i = sizeof(pi->data);
    memcpy(pi->data, task->cmdline, i);
    pi->len = i;
    pi->inode.size = i;
}

static struct proc_inode *proc_alloc(pid_t pid, int kind)
{
    struct proc_inode *pi;
    struct task_struct *task;

    task = proc_find_task(pid);
    if (!task)
        return NULL;

    pi = alloc_pages(0);
    if (!pi)
        return NULL;

    memset(pi, 0, sizeof(*pi));
    pi->magic = PROC_MAGIC;
    pi->pid = pid;
    pi->kind = kind;
    pi->inode.ino = 0x10000UL + (unsigned long)pid * 4UL + (unsigned long)kind;
    pi->inode.nlink = 1;
    pi->inode.private_data = pi;

    if (kind == PROC_KIND_DIR) {
        pi->inode.type = S_IFDIR;
        pi->inode.i_fop = &proc_pid_dir_ops;
        pi->inode.nlink = 2;
    } else {
        pi->inode.type = S_IFREG;
        pi->inode.i_fop = &proc_file_ops;
        if (kind == PROC_KIND_STAT)
            fill_stat(pi, task);
        else
            fill_cmdline(pi, task);
    }

    return pi;
}

static int parse_pid(const char *s, pid_t *out)
{
    unsigned long v = 0;

    if (!s || !*s)
        return -EINVAL;
    if (s[0] == 's' && s[1] == 'e' && s[2] == 'l' && s[3] == 'f' && !s[4]) {
        if (!current)
            return -ENOENT;
        *out = current->pid;
        return 0;
    }
    while (*s) {
        if (*s < '0' || *s > '9')
            return -EINVAL;
        v = v * 10UL + (unsigned long)(*s - '0');
        if (v > 0x7fffffffUL)
            return -EINVAL;
        s++;
    }
    if (v == 0)
        return -ENOENT;
    *out = (pid_t)v;
    return 0;
}

/*
 * Match /proc, /proc/<pid>, /proc/<pid>/stat|cmdline (trailing slash ok).
 */
struct inode *proc_lookup(const char *path)
{
    const char *p;
    char name[32];
    unsigned long n;
    pid_t pid;
    int err;
    struct proc_inode *pi;

    if (!path)
        return NULL;

    /* /proc or /proc/ */
    if (path[0] == '/' && path[1] == 'p' && path[2] == 'r' &&
        path[3] == 'o' && path[4] == 'c' &&
        (path[5] == '\0' || (path[5] == '/' && path[6] == '\0')))
        return proc_root_inode;

    if (!(path[0] == '/' && path[1] == 'p' && path[2] == 'r' &&
          path[3] == 'o' && path[4] == 'c' && path[5] == '/'))
        return NULL;

    p = path + 6;
    n = 0;
    while (p[n] && p[n] != '/' && n + 1 < sizeof(name)) {
        name[n] = p[n];
        n++;
    }
    name[n] = '\0';
    if (!name[0])
        return proc_root_inode;

    err = parse_pid(name, &pid);
    if (err)
        return NULL;

    if (!p[n] || (p[n] == '/' && p[n + 1] == '\0')) {
        pi = proc_alloc(pid, PROC_KIND_DIR);
        return pi ? &pi->inode : NULL;
    }

    if (p[n] != '/')
        return NULL;
    p += n + 1;

    if (p[0] == 's' && p[1] == 't' && p[2] == 'a' && p[3] == 't' &&
        (p[4] == '\0' || (p[4] == '/' && p[5] == '\0'))) {
        pi = proc_alloc(pid, PROC_KIND_STAT);
        return pi ? &pi->inode : NULL;
    }

    if (p[0] == 'c' && p[1] == 'm' && p[2] == 'd' && p[3] == 'l' &&
        p[4] == 'i' && p[5] == 'n' && p[6] == 'e' &&
        (p[7] == '\0' || (p[7] == '/' && p[8] == '\0'))) {
        pi = proc_alloc(pid, PROC_KIND_CMDLINE);
        return pi ? &pi->inode : NULL;
    }

    return NULL;
}

static int emit_dirent(void *dirp, unsigned long count, unsigned long *written,
                       long *pos, unsigned long ino, unsigned char type,
                       const char *name)
{
    unsigned long namelen = strlen(name);
    unsigned long reclen;
    unsigned long i;
    char kbuf[sizeof(struct linux_dirent64) + 32];
    struct linux_dirent64 *de = (struct linux_dirent64 *)kbuf;

    reclen = (offsetof(struct linux_dirent64, d_name) + namelen + 1UL + 7UL) &
             ~7UL;
    if (reclen > sizeof(kbuf))
        return -EINVAL;
    if (*written + reclen > count) {
        if (*written == 0)
            return -EINVAL;
        return 1; /* stop */
    }

    for (i = 0; i < reclen; i++)
        kbuf[i] = 0;
    de->d_ino = ino;
    de->d_off = *pos + 1;
    de->d_reclen = (unsigned short)reclen;
    de->d_type = type;
    for (i = 0; i < namelen; i++)
        de->d_name[i] = name[i];
    de->d_name[namelen] = '\0';

    if (copy_to_user((char *)dirp + *written, kbuf, reclen))
        return -EFAULT;

    *written += reclen;
    (*pos)++;
    return 0;
}

static long proc_root_readdir(struct file *file, void *dirp, unsigned long count)
{
    struct list_head *pos;
    struct task_struct *walk;
    long index;
    long pos_num;
    unsigned long written;
    unsigned long flags;
    int ret;

    if (!file || !dirp)
        return -EFAULT;
    if (count == 0)
        return 0;

    index = file->f_pos;
    written = 0;
    pos_num = 0;

    task_list_lock_irqsave(&flags);
    for_each_task(pos, walk) {
        char name[16];
        unsigned long v;
        int n = 0;

        if (walk->pid <= 0)
            continue;
        if (walk->state == TASK_DEAD || walk->state == TASK_ZOMBIE)
            continue;
        if (!walk->is_user)
            continue;

        if (pos_num < index) {
            pos_num++;
            continue;
        }

        v = (unsigned long)walk->pid;
        if (v == 0) {
            name[n++] = '0';
        } else {
            char tmp[16];
            int t = 0;

            while (v && t < (int)sizeof(tmp)) {
                tmp[t++] = '0' + (char)(v % 10);
                v /= 10;
            }
            while (t > 0)
                name[n++] = tmp[--t];
        }
        name[n] = '\0';

        task_list_unlock_irqrestore(flags);
        ret = emit_dirent(dirp, count, &written, &pos_num,
                          0x10000UL + (unsigned long)walk->pid * 4UL,
                          DT_DIR, name);
        if (ret < 0)
            return ret;
        if (ret > 0)
            break;

        file->f_pos = pos_num;
        task_list_lock_irqsave(&flags);
    }
    task_list_unlock_irqrestore(flags);

    return (long)written;
}

static long proc_pid_readdir(struct file *file, void *dirp, unsigned long count)
{
    static const char *const names[] = { "stat", "cmdline" };
    static const unsigned char types[] = { DT_REG, DT_REG };
    long index;
    long pos;
    unsigned long written;
    unsigned int i;
    int ret;
    struct proc_inode *pi;

    if (!file || !file->inode || !dirp)
        return -EFAULT;
    if (!proc_is_inode(file->inode))
        return -ENOTDIR;

    pi = PROC_I(file->inode);
    index = file->f_pos;
    written = 0;
    pos = 0;

    for (i = 0; i < 2; i++) {
        if (pos < index) {
            pos++;
            continue;
        }
        ret = emit_dirent(dirp, count, &written, &pos,
                          pi->inode.ino + 1UL + i, types[i], names[i]);
        if (ret < 0)
            return ret;
        if (ret > 0)
            break;
        file->f_pos = pos;
    }

    return (long)written;
}

static long proc_file_read(struct file *file, char *buf, unsigned long count,
                           long *pos)
{
    struct proc_inode *pi;
    unsigned long avail;
    unsigned long n;

    if (!file || !file->inode || !proc_is_inode(file->inode))
        return -EINVAL;
    if (!buf || !pos)
        return -EINVAL;

    pi = PROC_I(file->inode);
    if (*pos < 0)
        return -EINVAL;
    if ((unsigned long)*pos >= pi->len)
        return 0;

    avail = pi->len - (unsigned long)*pos;
    n = count < avail ? count : avail;
    memcpy(buf, pi->data + (unsigned long)*pos, n);
    *pos += (long)n;
    return (long)n;
}

static int proc_release(struct file *file)
{
    if (file && file->inode && proc_is_inode(file->inode)) {
        proc_iput(file->inode);
        file->inode = NULL;
    }
    return 0;
}

static struct file_ops proc_root_ops = {
    .readdir = proc_root_readdir,
};

static struct file_ops proc_pid_dir_ops = {
    .readdir = proc_pid_readdir,
    .release = proc_release,
};

static struct file_ops proc_file_ops = {
    .read = proc_file_read,
    .llseek = generic_file_llseek,
    .release = proc_release,
};

void proc_init(void)
{
    int err;

    err = ramfs_mkdir("/proc");
    if (err && err != -EEXIST)
        return;

    proc_root_inode = ramfs_lookup("/proc");
    if (!proc_root_inode)
        return;

    proc_root_inode->i_fop = &proc_root_ops;
}

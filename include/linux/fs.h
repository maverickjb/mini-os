#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <linux/types.h>

struct file;
struct task_struct;
struct dentry;
struct inode;

typedef unsigned short umode_t;

#define PATH_MAX        256

#define S_IFIFO         0010000
#define S_IFCHR         0020000
#define S_IFDIR         0040000
#define S_IFREG         0100000

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* File open mode bits (subset of Linux fmode_t). */
#define FMODE_LSEEK     0x4

struct file_ops {
    long (*read)(struct file *file, char *buf, unsigned long count, long *pos);
    long (*write)(struct file *file, const char *buf, unsigned long count,
                  long *pos);
    long (*readdir)(struct file *file, void *dirp, unsigned long count);
    long (*ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
    loff_t (*llseek)(struct file *file, loff_t offset, int whence);
    int (*release)(struct file *file);
};

struct inode_operations {
    int (*mkdir)(struct inode *dir, struct dentry *dentry, umode_t mode);
    int (*unlink)(struct inode *dir, struct dentry *dentry);
    int (*rmdir)(struct inode *dir, struct dentry *dentry);
    int (*link)(struct dentry *old_dentry, struct inode *dir,
                 struct dentry *new_dentry);
};

struct inode {
    unsigned long ino;
    unsigned long size;
    int type;
    const struct inode_operations *i_op;
    const struct file_ops *i_fop;
    void *private_data;
    unsigned int nlink;
};

struct file {
    int refcount;
    struct inode *inode;
    struct file_ops *f_op;
    void *private_data;
    long f_pos;
    int f_flags;
    fmode_t f_mode;
};

void get_file(struct file *file);
void fput(struct file *file);
struct file *alloc_file(void);
int install_fd(struct task_struct *task, struct file *file);

loff_t generic_file_llseek(struct file *file, loff_t offset, int whence);
loff_t vfs_llseek(struct file *file, loff_t offset, int whence);

/* Linux open flags (subset). */
#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_ACCMODE       3
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_DIRECTORY     0x10000

#define AT_FDCWD        (-100)
#define AT_REMOVEDIR    0x200

static inline int inode_is_dir(const struct inode *inode)
{
    return inode && inode->type == S_IFDIR;
}

static inline int inode_is_reg(const struct inode *inode)
{
    return inode && inode->type == S_IFREG;
}

#endif /* _LINUX_FS_H */

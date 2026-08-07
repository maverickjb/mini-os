#ifndef _LINUX_FS_H
#define _LINUX_FS_H

struct file;

#define S_IFDIR         0040000
#define S_IFREG         0100000

struct file_ops {
    long (*read)(struct file *file, char *buf, unsigned long count, long *pos);
    long (*write)(struct file *file, const char *buf, unsigned long count,
                  long *pos);
};

struct inode {
    unsigned long ino;
    unsigned long size;
    int type;
    struct file_ops *ops;
    void *private_data;
};

struct file {
    struct inode *inode;
    struct file_ops *f_op;
    void *private_data;
    long f_pos;
    int f_flags;
};

/* Linux open flags (subset). */
#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_ACCMODE       3
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_DIRECTORY     0x10000

#define AT_FDCWD        (-100)

static inline int inode_is_dir(const struct inode *inode)
{
    return inode && inode->type == S_IFDIR;
}

static inline int inode_is_reg(const struct inode *inode)
{
    return inode && inode->type == S_IFREG;
}

#endif /* _LINUX_FS_H */

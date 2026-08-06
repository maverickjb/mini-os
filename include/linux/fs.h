#ifndef _LINUX_FS_H
#define _LINUX_FS_H

struct file;

struct inode {
    unsigned long ino;      // inode number
    unsigned long size;     // file size

    int type;               // file/dir

    void *data;             // file content in memory

    struct file_ops *ops;
};

struct file_operations {
    long (*read)(struct file *file, char *buf, unsigned long count, long *pos);
    long (*write)(struct file *file, const char *buf, unsigned long count,
                  long *pos);
};

struct file {
    struct inode *inode;
    const struct file_operations *f_op;
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

#endif /* _LINUX_FS_H */

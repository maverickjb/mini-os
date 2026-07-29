#ifndef _LINUX_FS_H
#define _LINUX_FS_H

struct file;

struct file_operations {
    long (*write)(struct file *file, const char *buf, unsigned long count,
                  long *pos);
};

struct file {
    const struct file_operations *f_op;
    void *private_data;
    long f_pos;
};

#endif /* _LINUX_FS_H */

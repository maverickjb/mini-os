#ifndef _LINUX_PROC_FS_H
#define _LINUX_PROC_FS_H

struct inode;

void proc_init(void);
struct inode *proc_lookup(const char *path);
int proc_is_inode(const struct inode *inode);

#endif /* _LINUX_PROC_FS_H */

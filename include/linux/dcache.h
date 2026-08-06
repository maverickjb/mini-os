struct dentry {
    char name[32];

    struct inode *inode;

    struct dentry *parent;

    struct dentry *next;
};
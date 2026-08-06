struct inode *vfs_lookup(char *path)
{
    return ramfs_lookup(path);
}
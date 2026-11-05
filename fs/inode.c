/*
 * Inode refcount — i_count for open/lookup, nlink for names.
 * Last iput of an unlinked or non-persistent inode calls i_free.
 */

#include <linux/fs.h>

void iget(struct inode *inode)
{
    if (inode)
        inode->i_count++;
}

static void inode_maybe_evict(struct inode *inode)
{
    if (!inode || inode->i_count)
        return;
    if ((inode->i_flags & I_PERSISTENT) && inode->nlink)
        return;
    if (inode->i_free)
        inode->i_free(inode);
}

void drop_nlink(struct inode *inode)
{
    if (!inode)
        return;
    if (inode->nlink)
        inode->nlink--;
    inode_maybe_evict(inode);
}

void clear_nlink(struct inode *inode)
{
    if (!inode)
        return;
    inode->nlink = 0;
    inode_maybe_evict(inode);
}

void iput(struct inode *inode)
{
    if (!inode || inode->i_count == 0)
        return;
    inode->i_count--;
    inode_maybe_evict(inode);
}

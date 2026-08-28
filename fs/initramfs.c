/*
 * Initramfs unpack — Linux-style cpio "newc" to ramfs root.
 */

#include <linux/initramfs.h>
#include <linux/ramfs.h>
#include <linux/errno.h>
#include <linux/fs.h>

#define CPIO_MAGIC      "070701"
#define CPIO_TRAILER    "TRAILER!!!"
#define CPIO_HDR_SIZE   110

#define S_IFMT          00170000U

static unsigned long cpio_hex(const char *s, unsigned int len)
{
    unsigned long v = 0;
    unsigned int i;

    for (i = 0; i < len; i++) {
        char c = s[i];

        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f')
            v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v |= (unsigned long)(c - 'A' + 10);
    }

    return v;
}

static int cpio_streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static const unsigned char *cpio_align_up_ptr(const unsigned char *p)
{
    unsigned long v = (unsigned long)p;

    v = (v + 3) & ~3UL;
    return (const unsigned char *)v;
}

static int path_normalize(const char *name, char *out, unsigned long out_len)
{
    unsigned long i = 0;
    unsigned long j = 0;

    if (!name || !out || out_len < 2)
        return -EINVAL;

    if (name[0] == '.' && name[1] == '/')
        name += 2;
    else if (name[0] == '.')
        name++;

    out[j++] = '/';
    while (name[i]) {
        if (j + 1 >= out_len)
            return -EINVAL;
        out[j++] = name[i++];
    }
    out[j] = '\0';
    return 0;
}

static int ramfs_mkdir_p(const char *path)
{
    char partial[RAMFS_NAME_MAX + 2];
    unsigned long i = 1;
    int err;

    if (!path || path[0] != '/')
        return -EINVAL;

    if (path[1] == '\0')
        return 0;

    partial[0] = '/';
    partial[1] = '\0';

    while (path[i]) {
        unsigned long j = 1;

        if (path[i] == '/')
            i++;

        while (path[i] && path[i] != '/')
            partial[j++] = path[i++];

        partial[j] = '\0';
        if (j == 1)
            continue;

        err = ramfs_mkdir(partial);
        if (err && err != -EEXIST)
            return err;
    }

    return 0;
}

static int parent_dir(const char *path, char *dir)
{
    unsigned long len;
    unsigned long i;

    if (!path || path[0] != '/')
        return -EINVAL;

    len = 0;
    while (path[len])
        len++;

    i = len;
    while (i > 1 && path[i - 1] != '/')
        i--;

    if (i <= 1) {
        dir[0] = '/';
        dir[1] = '\0';
        return 0;
    }

    if (i >= RAMFS_NAME_MAX + 1)
        return -EINVAL;

    {
        unsigned long j;

        for (j = 0; j < i; j++)
            dir[j] = path[j];
        dir[i] = '\0';
    }

    return 0;
}

int unpack_to_rootfs(const void *data, unsigned long size)
{
    const unsigned char *p = data;
    const unsigned char *end = p + size;
    char path[RAMFS_NAME_MAX + 2];
    char pdir[RAMFS_NAME_MAX + 2];
    int err;

    if (!data)
        return -EINVAL;

    while (p + CPIO_HDR_SIZE <= end) {
        const char *hdr = (const char *)p;
        unsigned long namesize;
        unsigned long filesize;
        unsigned long mode;
        const char *name;
        const unsigned char *payload;

        if (hdr[0] != '0' || hdr[1] != '7' || hdr[2] != '0' ||
            hdr[3] != '7' || hdr[4] != '0' ||
            (hdr[5] != '1' && hdr[5] != '2'))
            return -EINVAL;

        namesize = cpio_hex(hdr + 94, 8);
        filesize = cpio_hex(hdr + 54, 8);
        mode = cpio_hex(hdr + 14, 8);

        p += CPIO_HDR_SIZE;
        if (p + namesize > end)
            return -EINVAL;

        name = (const char *)p;
        if (namesize == 0)
            return -EINVAL;

        p += namesize;
        p = cpio_align_up_ptr(p);
        if (p > end)
            return -EINVAL;

        payload = p;
        p += filesize;
        p = cpio_align_up_ptr(p);
        if (p > end)
            return -EINVAL;

        if (cpio_streq(name, CPIO_TRAILER))
            break;

        if (cpio_streq(name, ".") || cpio_streq(name, ".."))
            continue;

        if (path_normalize(name, path, sizeof(path)) < 0)
            return -EINVAL;

        if ((mode & S_IFMT) == S_IFDIR) {
            err = ramfs_mkdir(path);
            if (err && err != -EEXIST)
                return err;
            continue;
        }

        if ((mode & S_IFMT) == S_IFREG) {
            struct inode *inode;

            if (parent_dir(path, pdir) < 0)
                return -EINVAL;

            err = ramfs_mkdir_p(pdir);
            if (err)
                return err;

            err = ramfs_create(path);
            if (err && err != -EEXIST)
                return err;

            inode = ramfs_lookup(path);
            if (!inode)
                return -ENOENT;

            if (filesize) {
                long w = ramfs_write(inode, payload, filesize, 0);

                if (w < 0 || (unsigned long)w != filesize)
                    return (int)w;
            }

            continue;
        }

        if ((mode & S_IFMT) == S_IFLNK) {
            char target[RAMFS_NAME_MAX + 1];
            unsigned long tlen = filesize;
            unsigned long i;

            if (tlen >= sizeof(target))
                return -EINVAL;

            if (parent_dir(path, pdir) < 0)
                return -EINVAL;

            err = ramfs_mkdir_p(pdir);
            if (err)
                return err;

            for (i = 0; i < tlen; i++)
                target[i] = (char)payload[i];
            while (tlen > 0 && target[tlen - 1] == '\0')
                tlen--;
            target[tlen] = '\0';

            err = ramfs_symlink(path, target);
            if (err && err != -EEXIST)
                return err;

            continue;
        }

        /* Ignore devices, etc. */
    }

    return 0;
}

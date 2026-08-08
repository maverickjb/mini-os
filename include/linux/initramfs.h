#ifndef _LINUX_INITRAMFS_H
#define _LINUX_INITRAMFS_H

/*
 * Unpack a cpio newc initramfs image into the ramfs root.
 * Returns 0 on success, negative errno on failure.
 */
int unpack_to_rootfs(const void *data, unsigned long size);

#endif /* _LINUX_INITRAMFS_H */

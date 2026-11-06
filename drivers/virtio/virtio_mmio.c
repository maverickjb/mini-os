/*
 * Virtio MMIO probe — QEMU virt transports at 0x0a000000.
 *
 * Scan only: no virtqueues, no IRQ, no packets.
 */

#include <drivers/virtio/virtio_mmio.h>
#include <linux/printk.h>
#include <asm/memory.h>

#define VIRTIO_MMIO_MAGIC_VALUE    0x000
#define VIRTIO_MMIO_VERSION        0x004
#define VIRTIO_MMIO_DEVICE_ID      0x008
#define VIRTIO_MMIO_VENDOR_ID      0x00c

#define VIRTIO_MMIO_MAGIC          0x74726976U  /* "virt" */

#define VIRTIO_ID_NET              1U
#define VIRTIO_ID_BLOCK            2U

#define VIRTIO_MMIO_PHYS           0x0a000000UL
#define VIRTIO_MMIO_SIZE           0x200UL
#define VIRTIO_MMIO_COUNT          32U

struct virtio_mmio {
    unsigned long base;
    unsigned int version;
    unsigned int device_id;
    unsigned int vendor_id;
};

static unsigned int mmio_read32(unsigned long addr)
{
    return *(volatile unsigned int *)addr;
}

static int virtio_probe(unsigned long base, struct virtio_mmio *dev)
{
    unsigned int magic;

    magic = mmio_read32(base + VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC)
        return -1;

    dev->base = base;
    dev->version = mmio_read32(base + VIRTIO_MMIO_VERSION);
    dev->device_id = mmio_read32(base + VIRTIO_MMIO_DEVICE_ID);
    dev->vendor_id = mmio_read32(base + VIRTIO_MMIO_VENDOR_ID);

    if (dev->device_id == 0)
        return -1;

    return 0;
}

static const char *virtio_device_name(unsigned int id)
{
    switch (id) {
    case VIRTIO_ID_NET:
        return "network";
    case VIRTIO_ID_BLOCK:
        return "block";
    default:
        return "unknown";
    }
}

void virtio_mmio_init(void)
{
    struct virtio_mmio dev;
    unsigned int i;
    unsigned int found = 0;

    for (i = 0; i < VIRTIO_MMIO_COUNT; i++) {
        unsigned long phys = VIRTIO_MMIO_PHYS + i * VIRTIO_MMIO_SIZE;
        unsigned long base = (unsigned long)__phys_to_virt(phys);

        if (virtio_probe(base, &dev) != 0)
            continue;

        found++;
        pr_info("virtio: slot=%u phys=%lx version=%u id=%u (%s) vendor=%x\n",
                i, phys, dev.version, dev.device_id,
                virtio_device_name(dev.device_id), dev.vendor_id);
    }

    if (!found)
        pr_info("virtio: no devices (add -device virtio-net-device)\n");
}

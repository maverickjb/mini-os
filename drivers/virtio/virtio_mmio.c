/*
 * Virtio MMIO probe — QEMU virt transports at 0x0a000000.
 *
 * Find the device, reset, ACKNOWLEDGE|DRIVER, print features + MAC.
 * No virtqueues, no IRQ, no packets.
 */

#include <drivers/virtio/virtio_mmio.h>
#include <linux/printk.h>
#include <asm/memory.h>

#define VIRTIO_MMIO_MAGIC_VALUE		0x000
#define VIRTIO_MMIO_VERSION		0x004
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_VENDOR_ID		0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL	0x014
#define VIRTIO_MMIO_QUEUE_SEL		0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define VIRTIO_MMIO_STATUS		0x070
#define VIRTIO_MMIO_CONFIG		0x100

#define VIRTIO_MMIO_MAGIC		0x74726976U  /* "virt" */

#define VIRTIO_ID_NET			1U
#define VIRTIO_ID_BLOCK			2U

#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1U
#define VIRTIO_CONFIG_S_DRIVER		2U

#define VIRTIO_NET_F_MAC		(1U << 5)

#define VIRTIO_MMIO_PHYS		0x0a000000UL
#define VIRTIO_MMIO_SIZE		0x200UL
#define VIRTIO_MMIO_COUNT		32U

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

static void mmio_write32(unsigned long addr, unsigned int val)
{
    *(volatile unsigned int *)addr = val;
}

static unsigned char mmio_read8(unsigned long addr)
{
    return *(volatile unsigned char *)addr;
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

static void virtio_net_read_config(struct virtio_mmio *dev)
{
    unsigned int feat0;
    unsigned int feat1;
    unsigned int q0;
    unsigned int q1;
    unsigned char mac[6];
    unsigned int i;

    /* QEMU virt still offers legacy mmio v1; feature/MAC/status offsets match v2. */

    /* Reset, then the two status bits that allow config reads. */
    mmio_write32(dev->base + VIRTIO_MMIO_STATUS, 0);
    mmio_write32(dev->base + VIRTIO_MMIO_STATUS,
                 VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

    mmio_write32(dev->base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    feat0 = mmio_read32(dev->base + VIRTIO_MMIO_DEVICE_FEATURES);
    mmio_write32(dev->base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    feat1 = mmio_read32(dev->base + VIRTIO_MMIO_DEVICE_FEATURES);

    mmio_write32(dev->base + VIRTIO_MMIO_QUEUE_SEL, 0);
    q0 = mmio_read32(dev->base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    mmio_write32(dev->base + VIRTIO_MMIO_QUEUE_SEL, 1);
    q1 = mmio_read32(dev->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    pr_info("virtio-net: features0=%x features1=%x qmax rx=%u tx=%u\n",
            feat0, feat1, q0, q1);

    if (!(feat0 & VIRTIO_NET_F_MAC)) {
        pr_info("virtio-net: no MAC feature\n");
        return;
    }

    for (i = 0; i < 6; i++)
        mac[i] = mmio_read8(dev->base + VIRTIO_MMIO_CONFIG + i);

    pr_info("virtio-net: mac=%x:%x:%x:%x:%x:%x\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
        if (dev.device_id == VIRTIO_ID_NET)
            virtio_net_read_config(&dev);
    }

    if (!found)
        pr_info("virtio: no devices (add -device virtio-net-device)\n");
}

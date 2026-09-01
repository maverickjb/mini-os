/*
 * SLUB-style kmalloc — per-size caches backed by the page allocator.
 *
 * Each page begins with a struct slab header; objects are carved from the
 * remainder and linked through a per-slab freelist.
 */

#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>

#define PAGE_MASK       (~(PAGE_SIZE - 1UL))
#define MIN_OBJECT_SIZE sizeof(void *)

static struct kmem_cache cache_32;
static struct kmem_cache cache_64;
static struct kmem_cache cache_128;
static struct kmem_cache cache_256;
static struct kmem_cache cache_512;
static struct kmem_cache cache_1024;
static struct kmem_cache cache_2048;

static unsigned int slab_object_offset(struct kmem_cache *cache)
{
    unsigned int off = sizeof(struct slab);
    unsigned int align = cache->object_size;

    if (align < MIN_OBJECT_SIZE)
        align = MIN_OBJECT_SIZE;

    return (off + align - 1U) & ~(align - 1U);
}

static unsigned int slab_objects_per_page(struct kmem_cache *cache)
{
    unsigned int off = slab_object_offset(cache);

    if (off >= PAGE_SIZE)
        return 0;

    return (PAGE_SIZE - off) / cache->object_size;
}

static struct slab *slab_from_object(void *obj)
{
    return (struct slab *)((unsigned long)obj & PAGE_MASK);
}

static void partial_unlink(struct kmem_cache *cache, struct slab *slab)
{
    struct slab **prev = &cache->partial;

    while (*prev) {
        if (*prev == slab) {
            *prev = slab->next;
            slab->next = NULL;
            return;
        }
        prev = &(*prev)->next;
    }
}

static void partial_link(struct kmem_cache *cache, struct slab *slab)
{
    slab->next = cache->partial;
    cache->partial = slab;
}

static struct slab *slab_create(struct kmem_cache *cache)
{
    struct slab *slab;
    unsigned char *base;
    unsigned char *obj;
    unsigned int off;
    unsigned int i;

    slab = alloc_pages(0);
    if (!slab)
        return NULL;

    off = slab_object_offset(cache);
    slab->magic = SLAB_MAGIC;
    slab->cache = cache;
    slab->freelist = NULL;
    slab->inuse = 0;
    slab->objects = slab_objects_per_page(cache);
    slab->next = NULL;

    if (!slab->objects) {
        free_pages(slab, 0);
        return NULL;
    }

    base = (unsigned char *)slab;
    for (i = 0; i < slab->objects; i++) {
        obj = base + off + i * cache->object_size;
        *(void **)obj = slab->freelist;
        slab->freelist = obj;
    }

    return slab;
}

static void slab_destroy(struct kmem_cache *cache, struct slab *slab)
{
    partial_unlink(cache, slab);
    free_pages(slab, 0);
}

static struct kmem_cache *cache_for_size(unsigned long size)
{
    if (size <= 32)
        return &cache_32;
    if (size <= 64)
        return &cache_64;
    if (size <= 128)
        return &cache_128;
    if (size <= 256)
        return &cache_256;
    if (size <= 512)
        return &cache_512;
    if (size <= 1024)
        return &cache_1024;
    if (size <= 2048)
        return &cache_2048;
    return NULL;
}

static void *alloc_large(unsigned long size)
{
    struct slab *slab;
    unsigned int order = 0;
    unsigned long bytes = PAGE_SIZE;

    if (size == 0)
        size = 1;

    while (bytes < size + sizeof(struct slab) && order < 15U) {
        order++;
        bytes <<= 1;
    }

    slab = alloc_pages((int)order);
    if (!slab)
        return NULL;

    slab->magic = SLAB_MAGIC;
    slab->cache = NULL;
    slab->freelist = NULL;
    slab->inuse = 1;
    slab->objects = order;
    slab->next = NULL;

    return (unsigned char *)slab + sizeof(struct slab);
}

static void free_large(void *obj)
{
    struct slab *slab = slab_from_object(obj);

    free_pages(slab, (int)slab->objects);
}

void kmem_cache_init(struct kmem_cache *cache, unsigned int size)
{
    if (size < MIN_OBJECT_SIZE)
        size = MIN_OBJECT_SIZE;

    cache->object_size = size;
    cache->partial = NULL;
    spin_lock_init(&cache->lock);
}

void slub_init(void)
{
    kmem_cache_init(&cache_32, 32);
    kmem_cache_init(&cache_64, 64);
    kmem_cache_init(&cache_128, 128);
    kmem_cache_init(&cache_256, 256);
    kmem_cache_init(&cache_512, 512);
    kmem_cache_init(&cache_1024, 1024);
    kmem_cache_init(&cache_2048, 2048);
}

void *kmalloc(unsigned long size)
{
    struct kmem_cache *cache;

    if (size == 0)
        size = 1;

    cache = cache_for_size(size);
    if (cache)
        return kmem_cache_alloc(cache);

    return alloc_large(size);
}

void *kmem_cache_alloc(struct kmem_cache *cache)
{
    unsigned long flags;
    struct slab *slab;
    void *obj;

    spin_lock_irqsave(&cache->lock, flags);

    slab = cache->partial;
    if (!slab || !slab->freelist) {
        slab = slab_create(cache);
        if (!slab) {
            spin_unlock_irqrestore(&cache->lock, flags);
            return NULL;
        }
        partial_link(cache, slab);
    }

    obj = slab->freelist;
    slab->freelist = *(void **)obj;
    slab->inuse++;

    if (!slab->freelist)
        partial_unlink(cache, slab);

    spin_unlock_irqrestore(&cache->lock, flags);
    return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
    unsigned long flags;
    struct slab *slab;

    if (!obj)
        return;

    slab = slab_from_object(obj);
    if (slab->magic != SLAB_MAGIC || slab->cache != cache)
        return;

    spin_lock_irqsave(&cache->lock, flags);

    *(void **)obj = slab->freelist;
    slab->freelist = obj;
    slab->inuse--;

    if (slab->inuse == 0) {
        slab_destroy(cache, slab);
    } else if (slab->inuse == slab->objects - 1U) {
        partial_link(cache, slab);
    }

    spin_unlock_irqrestore(&cache->lock, flags);
}

void kfree(void *obj)
{
    struct slab *slab;

    if (!obj)
        return;

    slab = slab_from_object(obj);
    if (slab->magic != SLAB_MAGIC)
        return;

    if (!slab->cache) {
        free_large(obj);
        return;
    }

    kmem_cache_free(slab->cache, obj);
}

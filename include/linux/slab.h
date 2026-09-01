#ifndef __LINUX_SLAB_H
#define __LINUX_SLAB_H

#include <linux/spinlock.h>

#define SLAB_MAGIC  0x534C5542UL /* "SLUB" */

struct kmem_cache;

struct slab {
    unsigned long magic;
    struct kmem_cache *cache;
    void *freelist;
    unsigned int inuse;
    unsigned int objects;
    struct slab *next;
};

struct kmem_cache {
    unsigned int object_size;
    struct slab *partial;
    spinlock_t lock;
};

void slub_init(void);
void kmem_cache_init(struct kmem_cache *cache, unsigned int size);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);
void *kmalloc(unsigned long size);
void kfree(void *obj);

#endif /* __LINUX_SLAB_H */

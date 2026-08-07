/*
 * Simple bump + free-list allocator backed by brk(2).
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct chunk {
    size_t size;
    int free;
    struct chunk *next;
};

static struct chunk *heap_head;
static void *heap_break;

static size_t align_up(size_t n)
{
    return (n + 15UL) & ~15UL;
}

void *sbrk(long increment)
{
    void *old;
    void *neu;

    old = brk((void *)0);
    if ((long)old < 0)
        return (void *)-1;

    if (increment == 0)
        return old;

    neu = brk((char *)old + increment);
    if (neu != (char *)old + increment)
        return (void *)-1;

    return old;
}

static int grow_heap(size_t need)
{
    void *old;
    void *neu;
    struct chunk *c;
    size_t total;

    if (!heap_break) {
        old = brk((void *)0);
        if ((long)old < 0)
            return -1;
        heap_break = old;
    } else {
        old = heap_break;
    }

    total = align_up(need + sizeof(struct chunk));
    if (total < 4096)
        total = 4096;

    neu = brk((char *)heap_break + total);
    if (neu != (char *)heap_break + total)
        return -1;

    c = (struct chunk *)old;
    c->size = total - sizeof(struct chunk);
    c->free = 1;
    c->next = NULL;

    if (!heap_head) {
        heap_head = c;
    } else {
        struct chunk *walk = heap_head;

        while (walk->next)
            walk = walk->next;
        walk->next = c;
    }

    heap_break = neu;
    return 0;
}

void *malloc(size_t size)
{
    struct chunk *c;
    size_t need;

    if (size == 0)
        return NULL;

    need = align_up(size);

    for (;;) {
        for (c = heap_head; c; c = c->next) {
            if (c->free && c->size >= need) {
                c->free = 0;
                return (void *)(c + 1);
            }
        }
        if (grow_heap(need) < 0)
            return NULL;
    }
}

void free(void *ptr)
{
    struct chunk *c;

    if (!ptr)
        return;

    c = ((struct chunk *)ptr) - 1;
    c->free = 1;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *p;

    total = nmemb * size;
    p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    struct chunk *c;
    void *p;
    size_t n;

    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    c = ((struct chunk *)ptr) - 1;
    if (c->size >= size)
        return ptr;

    p = malloc(size);
    if (!p)
        return NULL;
    n = c->size < size ? c->size : size;
    memcpy(p, ptr, n);
    free(ptr);
    return p;
}

void exit(int status)
{
    _exit(status);
}

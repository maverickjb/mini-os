#ifndef _LINUX_LIST_H
#define _LINUX_LIST_H

#include <linux/stddef.h>

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new_entry,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new_entry;
    new_entry->next = next;
    new_entry->prev = prev;
    prev->next = new_entry;
}

static inline void list_add(struct list_head *new_entry,
                            struct list_head *head)
{
    __list_add(new_entry, head, head->next);
}

static inline void list_add_tail(struct list_head *new_entry,
                                 struct list_head *head)
{
    __list_add(new_entry, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/*
 * Always remove with list_del_init(); INIT_LIST_HEAD() before first insert.
 * Detached nodes are self-referring (next == entry), never NULL.
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

static inline int list_is_linked(const struct list_head *entry)
{
    return entry->next != entry;
}

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#endif /* _LINUX_LIST_H */

#ifndef _LINUX_RBTREE_H
#define _LINUX_RBTREE_H

#include <linux/stddef.h>

struct rb_node {
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    int color;
};

struct rb_root {
    struct rb_node *node;
};

#define RB_ROOT  (struct rb_root) { NULL }

static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
                                struct rb_node **link)
{
    node->parent = parent;
    node->left = NULL;
    node->right = NULL;
    node->color = 1;
    *link = node;
}

void rb_insert_color(struct rb_node *node, struct rb_root *root);
void rb_erase(struct rb_node *node, struct rb_root *root);

struct rb_node *rb_first(const struct rb_root *root);
struct rb_node *rb_next(const struct rb_node *node);

#define rb_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* _LINUX_RBTREE_H */

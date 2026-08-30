#include "test.h"

#include <linux/rbtree.h>

struct rb_int_node {
    struct rb_node node;
    int key;
};

static void rb_int_insert(struct rb_root *root, struct rb_int_node *new_node)
{
    struct rb_node **link = &root->node;
    struct rb_node *parent = NULL;
    struct rb_int_node *entry;

    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct rb_int_node, node);
        if (new_node->key < entry->key)
            link = &parent->left;
        else if (new_node->key > entry->key)
            link = &parent->right;
        else
            return;
    }

    rb_link_node(&new_node->node, parent, link);
    rb_insert_color(&new_node->node, root);
}

static struct rb_int_node *rb_int_search(struct rb_root *root, int key)
{
    struct rb_node *node = root->node;

    while (node) {
        struct rb_int_node *entry = rb_entry(node, struct rb_int_node, node);

        if (key < entry->key)
            node = node->left;
        else if (key > entry->key)
            node = node->right;
        else
            return entry;
    }

    return NULL;
}

int test_rbtree(void)
{
    struct rb_root root = RB_ROOT;
    struct rb_int_node nodes[5];
    struct rb_node *pos;
    int expect[] = { 1, 2, 3, 5, 7 };
    unsigned int i;

    for (i = 0; i < 5; i++) {
        nodes[i].key = expect[i];
        rb_int_insert(&root, &nodes[i]);
    }

    for (i = 0; i < 5; i++)
        EXPECT_TRUE(rb_int_search(&root, expect[i]) != NULL);

    EXPECT_TRUE(rb_int_search(&root, 4) == NULL);

    i = 0;
    for (pos = rb_first(&root); pos; pos = rb_next(pos)) {
        struct rb_int_node *entry = rb_entry(pos, struct rb_int_node, node);

        EXPECT_EQ(entry->key, expect[i]);
        i++;
    }

    EXPECT_EQ(i, 5U);

    rb_erase(&nodes[2].node, &root);
    EXPECT_TRUE(rb_int_search(&root, 3) == NULL);
    EXPECT_TRUE(rb_int_search(&root, 2) != NULL);
    EXPECT_TRUE(rb_int_search(&root, 5) != NULL);

    return 0;
}

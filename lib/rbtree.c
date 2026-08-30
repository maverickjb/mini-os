/*
 * Minimal red-black tree — insert, erase, in-order walk.
 */

#include <linux/rbtree.h>

#define RB_RED      1
#define RB_BLACK    0

static struct rb_node *rb_get_parent(const struct rb_node *node)
{
    return node->parent;
}

static int rb_is_red(const struct rb_node *node)
{
    return node && node->color == RB_RED;
}

static void rb_rotate_left(struct rb_root *root, struct rb_node *node)
{
    struct rb_node *right = node->right;

    node->right = right->left;
    if (right->left)
        right->left->parent = node;

    right->parent = node->parent;
    if (!node->parent)
        root->node = right;
    else if (node == node->parent->left)
        node->parent->left = right;
    else
        node->parent->right = right;

    right->left = node;
    node->parent = right;
}

static void rb_rotate_right(struct rb_root *root, struct rb_node *node)
{
    struct rb_node *left = node->left;

    node->left = left->right;
    if (left->right)
        left->right->parent = node;

    left->parent = node->parent;
    if (!node->parent)
        root->node = left;
    else if (node == node->parent->left)
        node->parent->left = left;
    else
        node->parent->right = left;

    left->right = node;
    node->parent = left;
}

void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *parent;
    struct rb_node *grandparent;

    while (1) {
        parent = rb_get_parent(node);
        if (!parent || parent->color == RB_BLACK)
            break;

        grandparent = rb_get_parent(parent);
        if (parent == grandparent->left) {
            struct rb_node *uncle = grandparent->right;

            if (rb_is_red(uncle)) {
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                grandparent->color = RB_RED;
                node = grandparent;
                continue;
            }

            if (node == parent->right) {
                rb_rotate_left(root, parent);
                node = parent;
                parent = rb_get_parent(node);
            }

            parent->color = RB_BLACK;
            grandparent->color = RB_RED;
            rb_rotate_right(root, grandparent);
        } else {
            struct rb_node *uncle = grandparent->left;

            if (rb_is_red(uncle)) {
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                grandparent->color = RB_RED;
                node = grandparent;
                continue;
            }

            if (node == parent->left) {
                rb_rotate_right(root, parent);
                node = parent;
                parent = rb_get_parent(node);
            }

            parent->color = RB_BLACK;
            grandparent->color = RB_RED;
            rb_rotate_left(root, grandparent);
        }
    }

    if (root->node)
        root->node->color = RB_BLACK;
}

static void rb_replace_node(struct rb_root *root, struct rb_node *victim,
                            struct rb_node *replacement)
{
    struct rb_node *parent = victim->parent;

    if (!parent)
        root->node = replacement;
    else if (victim == parent->left)
        parent->left = replacement;
    else
        parent->right = replacement;

    if (replacement)
        replacement->parent = parent;
}

void rb_erase(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *child;
    struct rb_node *parent;
    int color;

    if (!node->left)
        child = node->right;
    else if (!node->right)
        child = node->left;
    else {
        struct rb_node *succ = node->right;

        while (succ->left)
            succ = succ->left;

        if (node->right == succ) {
            child = succ->right;
            parent = succ;
            color = succ->color;
            if (child)
                child->parent = succ;
            succ->right = node->right;
            node->right->parent = succ;
        } else {
            child = succ->right;
            parent = succ->parent;
            color = succ->color;
            if (child)
                child->parent = parent;
            succ->parent = node->parent;
            if (node->parent) {
                if (node == node->parent->left)
                    node->parent->left = succ;
                else
                    node->parent->right = succ;
            } else {
                root->node = succ;
            }
            succ->left = node->left;
            node->left->parent = succ;
            succ->right = node->right;
            node->right->parent = succ;
        }

        rb_replace_node(root, node, succ);
        goto fixup;
    }

    rb_replace_node(root, node, child);
    parent = node->parent;
    color = node->color;

fixup:
    if (color == RB_BLACK) {
        while (child != root->node && !rb_is_red(child)) {
            if (child == parent->left) {
                struct rb_node *sibling = parent->right;

                if (rb_is_red(sibling)) {
                    sibling->color = RB_BLACK;
                    parent->color = RB_RED;
                    rb_rotate_left(root, parent);
                    sibling = parent->right;
                }
                if ((!sibling->left || sibling->left->color == RB_BLACK) &&
                    (!sibling->right || sibling->right->color == RB_BLACK)) {
                    sibling->color = RB_RED;
                    child = parent;
                    parent = child->parent;
                } else {
                    if (!sibling->right || sibling->right->color == RB_BLACK) {
                        if (sibling->left)
                            sibling->left->color = RB_BLACK;
                        sibling->color = RB_RED;
                        rb_rotate_right(root, sibling);
                        sibling = parent->right;
                    }
                    sibling->color = parent->color;
                    parent->color = RB_BLACK;
                    if (sibling->right)
                        sibling->right->color = RB_BLACK;
                    rb_rotate_left(root, parent);
                    child = root->node;
                    break;
                }
            } else {
                struct rb_node *sibling = parent->left;

                if (rb_is_red(sibling)) {
                    sibling->color = RB_BLACK;
                    parent->color = RB_RED;
                    rb_rotate_right(root, parent);
                    sibling = parent->left;
                }
                if ((!sibling->right || sibling->right->color == RB_BLACK) &&
                    (!sibling->left || sibling->left->color == RB_BLACK)) {
                    sibling->color = RB_RED;
                    child = parent;
                    parent = child->parent;
                } else {
                    if (!sibling->left || sibling->left->color == RB_BLACK) {
                        if (sibling->right)
                            sibling->right->color = RB_BLACK;
                        sibling->color = RB_RED;
                        rb_rotate_left(root, sibling);
                        sibling = parent->left;
                    }
                    sibling->color = parent->color;
                    parent->color = RB_BLACK;
                    if (sibling->left)
                        sibling->left->color = RB_BLACK;
                    rb_rotate_right(root, parent);
                    child = root->node;
                    break;
                }
            }
        }
        if (child)
            child->color = RB_BLACK;
    }
}

struct rb_node *rb_first(const struct rb_root *root)
{
    struct rb_node *node = root->node;

    if (!node)
        return NULL;

    while (node->left)
        node = node->left;

    return node;
}

struct rb_node *rb_next(const struct rb_node *node)
{
    struct rb_node *parent;

    if (node->right) {
        node = node->right;
        while (node->left)
            node = node->left;
        return (struct rb_node *)node;
    }

    while ((parent = node->parent) && node == parent->right)
        node = parent;

    return parent;
}

#include "test.h"

#include <linux/list.h>

struct test_list_item {
    struct list_head list;
    int val;
};

int test_list(void)
{
    LIST_HEAD(head);
    struct test_list_item a, b, c;
    struct list_head *pos;
    int sum = 0;

    INIT_LIST_HEAD(&a.list);
    INIT_LIST_HEAD(&b.list);
    INIT_LIST_HEAD(&c.list);

    a.val = 1;
    b.val = 2;
    c.val = 3;

    list_add_tail(&a.list, &head);
    list_add_tail(&b.list, &head);
    list_add_tail(&c.list, &head);

    EXPECT_TRUE(!list_empty(&head));

    list_for_each(pos, &head) {
        struct test_list_item *item =
            list_entry(pos, struct test_list_item, list);

        sum += item->val;
    }

    EXPECT_EQ(sum, 6);

    list_del_init(&b.list);
    EXPECT_TRUE(!list_empty(&head));

    sum = 0;
    list_for_each(pos, &head) {
        struct test_list_item *item =
            list_entry(pos, struct test_list_item, list);

        sum += item->val;
    }
    EXPECT_EQ(sum, 4);

    return 0;
}

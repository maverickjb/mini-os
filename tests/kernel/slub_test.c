#include "test.h"

#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/string.h>

int test_slub(void)
{
    void *a;
    void *b;
    void *large;
    unsigned char *walk;
    unsigned int i;

    a = kmalloc(32);
    EXPECT_TRUE(a != NULL);

    b = kmalloc(32);
    EXPECT_TRUE(b != NULL);
    EXPECT_TRUE(a != b);

    memset(a, 0xab, 32);
    memset(b, 0xcd, 32);
    EXPECT_TRUE(((unsigned char *)a)[0] == 0xab);
    EXPECT_TRUE(((unsigned char *)b)[0] == 0xcd);

    kfree(a);
    kfree(b);

    large = kmalloc(4096);
    EXPECT_TRUE(large != NULL);
    walk = large;
    for (i = 0; i < 4096; i++)
        walk[i] = (unsigned char)(i & 0xff);
    EXPECT_TRUE(walk[127] == 127);
    kfree(large);

    return 0;
}

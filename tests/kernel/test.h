#ifndef TEST_H
#define TEST_H

#include <linux/kernel.h>
#include <linux/printk.h>

void run_kernel_tests(void);

void test_pass(const char *name);
void test_fail(const char *name);

#define EXPECT_EQ(a, b)                                      \
    do {                                                     \
        if ((a) != (b)) {                                    \
            printk("FAIL: %s:%d: %ld != %ld\n",             \
                   __FILE__, __LINE__,                      \
                   (long)(a), (long)(b));                   \
            return -1;                                      \
        }                                                    \
    } while (0)

#define EXPECT_TRUE(x)                                       \
    do {                                                     \
        if (!(x)) {                                          \
            printk("FAIL: %s:%d\n", __FILE__, __LINE__);    \
            return -1;                                      \
        }                                                    \
    } while (0)

#endif

#include "test.h"

struct test_case {
    const char *name;
    int (*fn)(void);
};

int test_list(void);
int test_rbtree(void);
int test_spinlock(void);
int test_waitqueue(void);
int test_scheduler(void);

static struct test_case tests[] = {
    { "list",       test_list },
    { "rbtree",     test_rbtree },
    { "spinlock",   test_spinlock },
    { "waitqueue",  test_waitqueue },
    { "scheduler",  test_scheduler },
};

void test_pass(const char *name)
{
    printk("ok - %s\n", name);
}

void test_fail(const char *name)
{
    printk("not ok - %s\n", name);
}

void run_kernel_tests(void)
{
    unsigned int i;
    unsigned int passed = 0;
    unsigned int failed = 0;

    printk("TAP version 13\n");
    printk("1..%u\n", (unsigned int)ARRAY_SIZE(tests));

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        if (tests[i].fn() == 0) {
            printk("ok %u - %s\n", i + 1, tests[i].name);
            passed++;
        } else {
            printk("not ok %u - %s\n", i + 1, tests[i].name);
            failed++;
        }
    }

    printk("\n");
    printk("passed: %u\n", passed);
    printk("failed: %u\n", failed);
}

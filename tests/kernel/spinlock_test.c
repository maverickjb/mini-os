#include "test.h"

#include <linux/spinlock.h>

int test_spinlock(void)
{
    spinlock_t lock;

    spin_lock_init(&lock);

    spin_lock(&lock);

    EXPECT_TRUE(spin_is_locked(&lock));

    spin_unlock(&lock);

    EXPECT_TRUE(!spin_is_locked(&lock));

    return 0;
}

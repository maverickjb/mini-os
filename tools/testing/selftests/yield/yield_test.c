// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal kselftest: sched_yield(2) must succeed.
 *
 * Layout mirrors Linux tools/testing/selftests/<suite>/<test>.c
 */

#include <sched.h>

#include "../kselftest.h"

int main(void)
{
	int i;
	int ret;

	ksft_print_header();
	ksft_set_plan(2);

	ret = sched_yield();
	ksft_test_result(ret == 0, "sched_yield returns 0\n");

	for (i = 0; i < 100; i++) {
		ret = sched_yield();
		if (ret != 0)
			break;
	}
	ksft_test_result(ret == 0, "sched_yield x100\n");

	ksft_finished();
}

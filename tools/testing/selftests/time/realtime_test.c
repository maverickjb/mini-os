// SPDX-License-Identifier: GPL-2.0
/*
 * CLOCK_REALTIME from PL031 + generic timer (get_real_time).
 */

#include <time.h>

#include "../kselftest.h"

int main(void)
{
	struct timespec rt, rt2, mono;

	ksft_print_header();
	ksft_set_plan(3);

	ksft_test_result(clock_gettime(CLOCK_REALTIME, &rt) == 0 &&
			 rt.tv_sec > 1700000000L &&
			 rt.tv_nsec >= 0 && rt.tv_nsec < 1000000000L,
			 "CLOCK_REALTIME is a Unix wall clock\n");

	clock_gettime(CLOCK_MONOTONIC, &mono);
	ksft_test_result(mono.tv_sec >= 0 && mono.tv_sec < rt.tv_sec,
			 "CLOCK_MONOTONIC is uptime, not wall clock\n");

	clock_gettime(CLOCK_REALTIME, &rt2);
	ksft_test_result(rt2.tv_sec > rt.tv_sec ||
			 (rt2.tv_sec == rt.tv_sec && rt2.tv_nsec >= rt.tv_nsec),
			 "CLOCK_REALTIME is monotonic across two reads\n");

	ksft_finished();
}

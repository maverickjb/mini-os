// SPDX-License-Identifier: GPL-2.0
/*
 * CLOCK_REALTIME from PL031 + generic timer; clock_settime.
 */

#include <errno.h>
#include <time.h>

#include "../kselftest.h"

int main(void)
{
	struct timespec rt, rt2, mono, orig, set, got;

	ksft_print_header();
	ksft_set_plan(6);

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

	errno = 0;
	ksft_test_result(clock_settime(CLOCK_MONOTONIC, &rt) < 0 &&
			 errno == EINVAL,
			 "clock_settime CLOCK_MONOTONIC is EINVAL\n");

	set.tv_sec = 1700000000L;
	set.tv_nsec = 1000000000L;
	errno = 0;
	ksft_test_result(clock_settime(CLOCK_REALTIME, &set) < 0 &&
			 errno == EINVAL,
			 "clock_settime rejects nsec >= 1e9\n");

	clock_gettime(CLOCK_REALTIME, &orig);
	set.tv_sec = 1700000000L;
	set.tv_nsec = 0;
	if (clock_settime(CLOCK_REALTIME, &set) != 0) {
		ksft_test_result_fail("clock_settime CLOCK_REALTIME\n");
	} else {
		clock_gettime(CLOCK_REALTIME, &got);
		ksft_test_result(got.tv_sec == set.tv_sec ||
				 got.tv_sec == set.tv_sec + 1,
				 "clock_settime is visible to gettime\n");
		clock_settime(CLOCK_REALTIME, &orig);
	}

	ksft_finished();
}

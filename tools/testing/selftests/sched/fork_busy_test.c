// SPDX-License-Identifier: GPL-2.0
/*
 * N busy fork children must all exit and be reaped — none stuck RUNNING.
 */

#include <sched.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../kselftest.h"

#define NCHILD		8
#define BUSY_ITERS	2000000UL
#define TIMEOUT_SEC	5

static int timed_out(const struct timespec *deadline)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 1;
	if (now.tv_sec != deadline->tv_sec)
		return now.tv_sec > deadline->tv_sec;
	return now.tv_nsec > deadline->tv_nsec;
}

static void child_busy(void)
{
	volatile unsigned long i;

	for (i = 0; i < BUSY_ITERS; i++)
		;
	_exit(0);
}

int main(void)
{
	pid_t pids[NCHILD];
	struct timespec deadline;
	int nfork;
	int nreaped;
	int i;

	ksft_print_header();
	ksft_set_plan(2);

	nfork = 0;
	for (i = 0; i < NCHILD; i++) {
		pids[i] = fork();
		if (pids[i] < 0)
			break;
		if (pids[i] == 0)
			child_busy();
		nfork++;
	}
	ksft_test_result(nfork == NCHILD, "fork %d busy children\n", NCHILD);

	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
		ksft_test_result_fail("clock_gettime\n");
		ksft_finished();
	}
	deadline.tv_sec += TIMEOUT_SEC;

	nreaped = 0;
	for (i = 0; i < nfork; i++) {
		int status;
		pid_t w;

		for (;;) {
			w = waitpid(pids[i], &status, WNOHANG);
			if (w == pids[i])
				break;
			if (w < 0 || timed_out(&deadline)) {
				ksft_print_msg("child pid %d not reaped\n",
					       (int)pids[i]);
				goto done;
			}
			sched_yield();
		}

		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			ksft_print_msg("child pid %d bad status %d\n",
				       (int)pids[i], status);
			goto done;
		}
		nreaped++;
	}

done:
	ksft_test_result(nreaped == NCHILD,
			 "all %d busy children exited (reaped %d)\n",
			 NCHILD, nreaped);
	ksft_finished();
}

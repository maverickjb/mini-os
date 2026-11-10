// SPDX-License-Identifier: GPL-2.0
/*
 * One nanosleep child must wake while a spinner stays RUNNING.
 */

#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../kselftest.h"

#define SLEEP_NS	200000000L	/* 200 ms */
#define TIMEOUT_SEC	3

static int timed_out(const struct timespec *deadline)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 1;
	if (now.tv_sec != deadline->tv_sec)
		return now.tv_sec > deadline->tv_sec;
	return now.tv_nsec > deadline->tv_nsec;
}

static int wait_pid_deadline(pid_t pid, int *status,
			     const struct timespec *deadline)
{
	pid_t w;

	for (;;) {
		w = waitpid(pid, status, WNOHANG);
		if (w == pid)
			return 0;
		if (w < 0 || timed_out(deadline))
			return -1;
		sched_yield();
	}
}

int main(void)
{
	pid_t spinner;
	pid_t sleeper;
	struct timespec deadline;
	struct timespec req;
	int status;
	int i;
	pid_t w;

	ksft_print_header();
	ksft_set_plan(3);

	spinner = fork();
	if (spinner == 0)
		/* Yield so SIGKILL can be delivered on syscall return. */
		for (;;)
			sched_yield();

	sleeper = fork();
	if (sleeper == 0) {
		req.tv_sec = 0;
		req.tv_nsec = SLEEP_NS;
		if (nanosleep(&req, NULL) != 0)
			_exit(2);
		_exit(0);
	}

	ksft_test_result(spinner > 0 && sleeper > 0,
			 "fork spinner and sleeper\n");
	if (spinner <= 0 || sleeper <= 0)
		ksft_finished();

	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
		ksft_test_result_fail("clock_gettime\n");
		kill(spinner, SIGKILL);
		ksft_finished();
	}
	deadline.tv_sec += TIMEOUT_SEC;

	if (wait_pid_deadline(sleeper, &status, &deadline) != 0) {
		ksft_test_result_fail("sleeper did not wake\n");
		kill(spinner, SIGKILL);
		(void)waitpid(sleeper, &status, 0);
		(void)waitpid(spinner, &status, 0);
		ksft_finished();
	}

	ksft_test_result(WIFEXITED(status) && WEXITSTATUS(status) == 0,
			 "sleeper woke and exited 0\n");

	for (i = 0; i < 20; i++)
		sched_yield();

	w = waitpid(spinner, &status, WNOHANG);
	ksft_test_result(w == 0, "spinner still running after sleeper woke\n");

	kill(spinner, SIGKILL);
	if (clock_gettime(CLOCK_MONOTONIC, &deadline) == 0)
		deadline.tv_sec += TIMEOUT_SEC;
	if (wait_pid_deadline(spinner, &status, &deadline) != 0)
		ksft_print_msg("spinner not reaped after SIGKILL\n");

	ksft_finished();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Two tasks ping-pong a shared turn/counter with sched_yield.
 * The file is the shared cell (anon mmap is COW, not MAP_SHARED).
 */

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../kselftest.h"

#define TARGET		200U
#define TIMEOUT_SEC	5
#define CTR_PATH	"/tmp/yield_pong"

struct pong {
	unsigned int turn;
	unsigned int n;
};

static int timed_out(const struct timespec *deadline)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 1;
	if (now.tv_sec != deadline->tv_sec)
		return now.tv_sec > deadline->tv_sec;
	return now.tv_nsec > deadline->tv_nsec;
}

static int load(int fd, struct pong *st)
{
	if (lseek(fd, 0, SEEK_SET) < 0)
		return -1;
	if (read(fd, st, sizeof(*st)) != (ssize_t)sizeof(*st))
		return -1;
	return 0;
}

static int store(int fd, const struct pong *st)
{
	if (lseek(fd, 0, SEEK_SET) < 0)
		return -1;
	if (write(fd, st, sizeof(*st)) != (ssize_t)sizeof(*st))
		return -1;
	return 0;
}

static int play(int fd, unsigned int me, const struct timespec *deadline)
{
	struct pong st;

	for (;;) {
		if (timed_out(deadline))
			return -1;
		if (load(fd, &st) < 0)
			return -1;
		if (st.n >= TARGET)
			return 0;
		if (st.turn != me) {
			sched_yield();
			continue;
		}
		st.n++;
		st.turn ^= 1U;
		if (store(fd, &st) < 0)
			return -1;
		sched_yield();
	}
}

int main(void)
{
	struct pong st;
	struct timespec deadline;
	pid_t pid;
	int fd;
	int status;
	int child_ok;
	int parent_ok;

	ksft_print_header();
	ksft_set_plan(2);

	fd = open(CTR_PATH, O_RDWR | O_CREAT | O_TRUNC, 0644);
	st.turn = 0;
	st.n = 0;
	if (fd < 0 || store(fd, &st) < 0) {
		ksft_test_result_fail("create shared counter file\n");
		ksft_finished();
	}

	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
		ksft_test_result_fail("clock_gettime\n");
		ksft_finished();
	}
	deadline.tv_sec += TIMEOUT_SEC;

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		ksft_finished();
	}
	if (pid == 0) {
		close(fd);
		fd = open(CTR_PATH, O_RDWR);
		if (fd < 0 || play(fd, 0, &deadline) != 0)
			_exit(2);
		_exit(0);
	}

	close(fd);
	fd = open(CTR_PATH, O_RDWR);
	parent_ok = fd >= 0 && play(fd, 1, &deadline) == 0;
	ksft_test_result(parent_ok, "parent reached %u via yield ping-pong\n",
			 TARGET);

	child_ok = 0;
	for (;;) {
		if (waitpid(pid, &status, WNOHANG) == pid) {
			child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
			break;
		}
		if (timed_out(&deadline)) {
			kill(pid, SIGKILL);
			(void)waitpid(pid, &status, 0);
			break;
		}
		sched_yield();
	}
	ksft_test_result(child_ok, "child reached %u via yield ping-pong\n",
			 TARGET);

	close(fd);
	unlink(CTR_PATH);
	ksft_finished();
}

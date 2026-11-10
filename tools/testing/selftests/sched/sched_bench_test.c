// SPDX-License-Identifier: GPL-2.0
/*
 * Measure scheduler rate: /proc/schedstat while 8 yield-hogs run.
 * QEMU TCG wall time is noisy — use the counts, not claimed ns/switch.
 */

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../kselftest.h"

#define NHOG		8
#define SAMPLE_NS	200000000L	/* 200 ms */
#define TIMEOUT_SEC	3

struct schedstat {
	unsigned long switches;
	unsigned long pulls;
	unsigned long ipis;
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

static int read_schedstat(struct schedstat *st)
{
	char buf[512];
	char *p;
	ssize_t n;
	int fd;

	st->switches = 0;
	st->pulls = 0;
	st->ipis = 0;

	fd = open("/proc/schedstat", O_RDONLY);
	if (fd < 0)
		return -1;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return -1;
	buf[n] = '\0';

	p = buf;
	while (*p) {
		unsigned long sw = 0, pulls = 0, ipis = 0;

		if (p[0] != 'c' || p[1] != 'p' || p[2] != 'u')
			return -1;
		while (*p && *p != ' ')
			p++;
		while (*p == ' ')
			p++;
		while (*p >= '0' && *p <= '9') {
			sw = sw * 10 + (unsigned long)(*p - '0');
			p++;
		}
		while (*p == ' ')
			p++;
		while (*p >= '0' && *p <= '9') {
			pulls = pulls * 10 + (unsigned long)(*p - '0');
			p++;
		}
		while (*p == ' ')
			p++;
		while (*p >= '0' && *p <= '9') {
			ipis = ipis * 10 + (unsigned long)(*p - '0');
			p++;
		}
		if (*p == '\n')
			p++;
		st->switches += sw;
		st->pulls += pulls;
		st->ipis += ipis;
	}
	return 0;
}

static long long ts_ns(const struct timespec *t)
{
	return (long long)t->tv_sec * 1000000000LL + t->tv_nsec;
}

static void reap_hogs(pid_t *pids, int n)
{
	struct timespec deadline;
	int i;
	int status;

	for (i = 0; i < n; i++)
		kill(pids[i], SIGKILL);
	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0)
		return;
	deadline.tv_sec += TIMEOUT_SEC;
	for (i = 0; i < n; i++) {
		while (waitpid(pids[i], &status, WNOHANG) != pids[i]) {
			if (timed_out(&deadline))
				return;
			sched_yield();
		}
	}
}

int main(void)
{
	pid_t pids[NHOG];
	struct schedstat a, b;
	struct timespec t0, t1, req;
	int nfork;
	int i;
	long long dt_ns;
	unsigned long dsw, dpull, dipi;
	unsigned long rate;

	ksft_print_header();
	ksft_set_plan(3);

	ksft_test_result(read_schedstat(&a) == 0 && a.switches > 0,
			 "/proc/schedstat readable (switches %lu)\n",
			 a.switches);

	nfork = 0;
	for (i = 0; i < NHOG; i++) {
		pids[i] = fork();
		if (pids[i] < 0)
			break;
		if (pids[i] == 0)
			for (;;)
				sched_yield();
		nfork++;
	}
	ksft_test_result(nfork == NHOG, "fork %d yield hogs\n", NHOG);

	if (read_schedstat(&a) != 0 ||
	    clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
		ksft_test_result_fail("snapshot before sample\n");
		reap_hogs(pids, nfork);
		ksft_finished();
	}

	req.tv_sec = 0;
	req.tv_nsec = SAMPLE_NS;
	(void)nanosleep(&req, NULL);

	if (read_schedstat(&b) != 0 ||
	    clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
		ksft_test_result_fail("snapshot after sample\n");
		reap_hogs(pids, nfork);
		ksft_finished();
	}

	dsw = b.switches - a.switches;
	dpull = b.pulls - a.pulls;
	dipi = b.ipis - a.ipis;
	dt_ns = ts_ns(&t1) - ts_ns(&t0);
	if (dt_ns <= 0)
		dt_ns = 1;
	rate = (unsigned long)((dsw * 1000000000ULL) / (unsigned long long)dt_ns);

	ksft_print_msg("sample %lld ns: +%lu switches +%lu pulls +%lu ipis => %lu switches/s\n",
		       dt_ns, dsw, dpull, dipi, rate);
	ksft_test_result(dsw > 0,
			 "switches increased under %d hogs (+%lu in sample)\n",
			 NHOG, dsw);

	reap_hogs(pids, nfork);
	ksft_finished();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Fork 8 CPU hogs on -smp 4. Fork placement should put user tasks on
 * every CPU (/proc/<pid>/stat field 39).
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
#define TIMEOUT_SEC	3
#define MIN_CPUS	4

static int timed_out(const struct timespec *deadline)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 1;
	if (now.tv_sec != deadline->tv_sec)
		return now.tv_sec > deadline->tv_sec;
	return now.tv_nsec > deadline->tv_nsec;
}

static void hog(void)
{
	/* Yield so idle pull and SIGKILL can run on syscall return. */
	for (;;)
		sched_yield();
}

static int read_processor(pid_t pid)
{
	char path[64];
	char buf[512];
	char *p;
	ssize_t n;
	int fd;
	int field;
	unsigned int cpu;

	if (snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid) < 0)
		return -1;
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return -1;
	buf[n] = '\0';

	p = strrchr(buf, ')');
	if (!p)
		return -1;
	p++;
	field = 2;
	while (*p) {
		while (*p == ' ')
			p++;
		if (!*p || *p == '\n')
			break;
		field++;
		if (field == 39) {
			if (*p < '0' || *p > '9')
				return -1;
			cpu = 0;
			while (*p >= '0' && *p <= '9') {
				cpu = cpu * 10 + (unsigned int)(*p - '0');
				p++;
			}
			return (int)cpu;
		}
		while (*p && *p != ' ' && *p != '\n')
			p++;
	}
	return -1;
}

static int unique_cpus(const pid_t *pids, int n)
{
	unsigned int seen = 0;
	int ncpu = 0;
	int i;
	int cpu;

	for (i = 0; i < n; i++) {
		cpu = read_processor(pids[i]);
		if (cpu < 0 || cpu >= 32)
			continue;
		if (seen & (1U << cpu))
			continue;
		seen |= 1U << cpu;
		ncpu++;
	}
	return ncpu;
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
	struct timespec deadline;
	int nfork;
	int nalive;
	int ncpu;
	int i;
	pid_t w;

	ksft_print_header();
	ksft_set_plan(3);

	nfork = 0;
	for (i = 0; i < NHOG; i++) {
		pids[i] = fork();
		if (pids[i] < 0)
			break;
		if (pids[i] == 0)
			hog();
		nfork++;
	}
	ksft_test_result(nfork == NHOG, "fork %d CPU hogs\n", NHOG);

	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
		ksft_test_result_fail("clock_gettime\n");
		ksft_test_result_fail("hogs spread across CPUs\n");
		reap_hogs(pids, nfork);
		ksft_finished();
	}
	deadline.tv_sec += TIMEOUT_SEC;

	ncpu = 0;
	while (!timed_out(&deadline)) {
		ncpu = unique_cpus(pids, nfork);
		if (ncpu >= MIN_CPUS)
			break;
		sched_yield();
	}

	nalive = 0;
	for (i = 0; i < nfork; i++) {
		w = waitpid(pids[i], NULL, WNOHANG);
		if (w == 0)
			nalive++;
	}
	ksft_test_result(nalive == NHOG,
			 "all %d hogs still running (alive %d)\n",
			 NHOG, nalive);

	ksft_test_result(ncpu >= MIN_CPUS,
			 "hogs observed on %d CPUs (want >= %d)\n",
			 ncpu, MIN_CPUS);

	reap_hogs(pids, nfork);
	ksft_finished();
}

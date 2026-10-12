// SPDX-License-Identifier: GPL-2.0
/*
 * SA_RESTART: a handler interrupting a blocking pipe read should rewind
 * the syscall. Without the flag, the same read must fail with EINTR.
 */

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../kselftest.h"

static volatile sig_atomic_t got;

static void on_usr1(int sig)
{
	(void)sig;
	got = 1;
}

static void test_eintr(void)
{
	struct sigaction sa;
	int fds[2], ready[2];
	pid_t pid;
	int status;
	char c;
	int i;

	if (pipe(fds) < 0 || pipe(ready) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		return;
	}

	if (pid == 0) {
		ssize_t n;

		sa.sa_handler = on_usr1;
		sa.sa_flags = 0;
		sa.sa_restorer = 0;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGUSR1, &sa, NULL) < 0)
			_exit(1);
		got = 0;
		close(fds[1]);
		close(ready[0]);
		if (write(ready[1], "r", 1) != 1)
			_exit(1);
		close(ready[1]);
		n = read(fds[0], &c, 1);
		if (got && n < 0 && errno == EINTR)
			_exit(0);
		_exit(2);
	}

	close(fds[0]);
	close(ready[1]);
	if (read(ready[0], &c, 1) != 1) {
		ksft_test_result_fail("ready\n");
		return;
	}
	close(ready[0]);
	for (i = 0; i < 10000; i++)
		sched_yield();
	if (kill(pid, SIGUSR1) < 0)
		ksft_test_result_fail("kill\n");
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("no SA_RESTART: expected EINTR\n");
	else
		ksft_test_result_pass("no SA_RESTART: read returns EINTR\n");
	close(fds[1]);
}

static void test_restart(void)
{
	struct sigaction sa;
	int fds[2], ready[2];
	pid_t pid;
	int status;
	char c;
	int i;

	if (pipe(fds) < 0 || pipe(ready) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		return;
	}

	if (pid == 0) {
		ssize_t n;

		sa.sa_handler = on_usr1;
		sa.sa_flags = SA_RESTART;
		sa.sa_restorer = 0;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGUSR1, &sa, NULL) < 0)
			_exit(1);
		got = 0;
		close(fds[1]);
		close(ready[0]);
		if (write(ready[1], "r", 1) != 1)
			_exit(1);
		close(ready[1]);
		n = read(fds[0], &c, 1);
		if (got && n == 1 && c == 'X')
			_exit(0);
		_exit(3);
	}

	close(fds[0]);
	close(ready[1]);
	if (read(ready[0], &c, 1) != 1) {
		ksft_test_result_fail("ready\n");
		return;
	}
	close(ready[0]);
	for (i = 0; i < 10000; i++)
		sched_yield();
	if (kill(pid, SIGUSR1) < 0)
		ksft_test_result_fail("kill\n");
	for (i = 0; i < 10000; i++)
		sched_yield();
	if (write(fds[1], "X", 1) != 1)
		ksft_test_result_fail("write\n");
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("SA_RESTART: expected restarted read\n");
	else
		ksft_test_result_pass("SA_RESTART: read completes after handler\n");
	close(fds[1]);
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(2);

	test_eintr();
	test_restart();

	ksft_finished();
}

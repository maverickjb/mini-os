// SPDX-License-Identifier: GPL-2.0
/*
 * Linux sigset layout: signal n is bit (n-1). sa_mask, sigprocmask, sigpending.
 */

#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../kselftest.h"

static volatile sig_atomic_t got_usr1;
static volatile sig_atomic_t got_usr2;
static volatile sig_atomic_t in_usr1;
static volatile sig_atomic_t usr2_nested;

static void on_usr1(int sig)
{
	(void)sig;
	got_usr1 = 1;
}

static void on_usr2(int sig)
{
	(void)sig;
	if (in_usr1)
		usr2_nested = 1;
	got_usr2 = 1;
}

static void on_usr1_mask_usr2(int sig)
{
	(void)sig;
	in_usr1 = 1;
	kill(getpid(), SIGUSR2);
	got_usr1 = 1;
	in_usr1 = 0;
}

static void test_sa_mask(void)
{
	struct sigaction sa;
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		return;
	}
	if (pid == 0) {
		sa.sa_handler = on_usr2;
		sa.sa_flags = 0;
		sa.sa_restorer = 0;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGUSR2, &sa, NULL) < 0)
			_exit(1);

		sa.sa_handler = on_usr1_mask_usr2;
		sa.sa_flags = 0;
		sa.sa_restorer = 0;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGUSR2);
		if (sigaction(SIGUSR1, &sa, NULL) < 0)
			_exit(1);

		got_usr1 = got_usr2 = usr2_nested = in_usr1 = 0;
		if (kill(getpid(), SIGUSR1) < 0)
			_exit(2);
		if (got_usr1 && got_usr2 && !usr2_nested)
			_exit(0);
		_exit(3);
	}

	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("sa_mask did not defer SIGUSR2\n");
	else
		ksft_test_result_pass("sa_mask defers SIGUSR2 until handler returns\n");
}

static void test_sigprocmask_block(void)
{
	struct sigaction sa;
	sigset_t set, old;
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		return;
	}
	if (pid == 0) {
		sa.sa_handler = on_usr1;
		sa.sa_flags = 0;
		sa.sa_restorer = 0;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGUSR1, &sa, NULL) < 0)
			_exit(1);

		got_usr1 = 0;
		sigemptyset(&set);
		sigaddset(&set, SIGUSR1);
		if (sigprocmask(SIG_BLOCK, &set, &old) < 0)
			_exit(2);
		if (kill(getpid(), SIGUSR1) < 0)
			_exit(3);
		if (got_usr1)
			_exit(4);
		if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
			_exit(5);
		if (!got_usr1)
			_exit(6);
		_exit(0);
	}

	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("sigprocmask block/unblock SIGUSR1\n");
	else
		ksft_test_result_pass("sigprocmask blocks SIGUSR1 until unblock\n");
}

static void test_sigpending(void)
{
	sigset_t set, pending;
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		return;
	}
	if (pid == 0) {
		sigemptyset(&set);
		sigaddset(&set, SIGUSR1);
		if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
			_exit(1);
		if (kill(getpid(), SIGUSR1) < 0)
			_exit(2);
		sigemptyset(&pending);
		if (sigpending(&pending) < 0)
			_exit(3);
		if (!sigismember(&pending, SIGUSR1))
			_exit(4);
		_exit(0);
	}

	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("sigpending (status=%d)\n", status);
	else
		ksft_test_result_pass("sigpending reports blocked SIGUSR1\n");
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(3);

	test_sa_mask();
	test_sigprocmask_block();
	test_sigpending();

	ksft_finished();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Basic anon COW after fork — inspired by Linux
 * tools/testing/selftests/mm/cow.c (basic cases only).
 *
 * Child modifies a shared anon page; parent must still see the pre-fork
 * bytes. Keep the child alive until after the parent check so SIGCHLD
 * cannot EINTR the pipe sync (no SA_RESTART yet).
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "../kselftest.h"

#define PAGE_SIZE 4096UL

static void test_cow_child_writes(void)
{
	char *mem;
	char *old;
	int go[2], done[2], die[2];
	pid_t pid;
	int status;
	char buf;

	mem = malloc(PAGE_SIZE);
	old = malloc(PAGE_SIZE);
	if (!mem || !old) {
		ksft_test_result_fail("malloc\n");
		free(mem);
		free(old);
		return;
	}
	memset(mem, 0x33, PAGE_SIZE);
	memcpy(old, mem, PAGE_SIZE);

	if (pipe(go) < 0 || pipe(done) < 0 || pipe(die) < 0) {
		ksft_test_result_fail("pipe\n");
		free(mem);
		free(old);
		return;
	}

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		free(mem);
		free(old);
		return;
	}

	if (pid == 0) {
		if (read(go[0], &buf, 1) != 1)
			_exit(1);
		memset(mem, 0x44, PAGE_SIZE);
		if (write(done[1], "0", 1) != 1)
			_exit(1);
		if (read(die[0], &buf, 1) != 1)
			_exit(1);
		_exit(0);
	}

	if (write(go[1], "0", 1) != 1 || read(done[0], &buf, 1) != 1) {
		ksft_test_result_fail("sync\n");
		(void)write(die[1], "0", 1);
		(void)waitpid(pid, &status, 0);
		free(mem);
		free(old);
		return;
	}

	ksft_test_result(memcmp(old, mem, PAGE_SIZE) == 0,
			 "child write: parent still sees old bytes\n");

	(void)write(die[1], "0", 1);
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("waitpid\n");

	free(mem);
	free(old);
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(1);

	test_cow_child_writes();

	ksft_finished();
}

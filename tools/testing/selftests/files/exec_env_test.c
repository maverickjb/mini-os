// SPDX-License-Identifier: GPL-2.0
/*
 * execve(2) must copy envp onto the new stack (not drop it).
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../kselftest.h"

int main(int argc, char **argv)
{
	pid_t pid;
	int status;
	char *av[3];
	char *ev[4];

	if (argc == 2 && strcmp(argv[1], "--child") == 0) {
		const char *foo = getenv("MINIOS_FOO");
		const char *tz = getenv("TZ");

		if (foo && strcmp(foo, "bar") == 0 &&
		    tz && strcmp(tz, "CST-8") == 0)
			_exit(0);
		_exit(2);
	}

	ksft_print_header();
	ksft_set_plan(1);

	av[0] = argv[0];
	av[1] = "--child";
	av[2] = NULL;
	ev[0] = "MINIOS_FOO=bar";
	ev[1] = "TZ=CST-8";
	ev[2] = "PATH=/bin";
	ev[3] = NULL;

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		ksft_finished();
	}
	if (pid == 0) {
		execve(argv[0], av, ev);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) != pid) {
		ksft_test_result_fail("waitpid\n");
		ksft_finished();
	}

	ksft_test_result(WIFEXITED(status) && WEXITSTATUS(status) == 0,
			 "execve copies envp (got status %d)\n",
			 WIFEXITED(status) ? WEXITSTATUS(status) : -1);

	ksft_finished();
}

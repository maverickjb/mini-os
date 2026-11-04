// SPDX-License-Identifier: GPL-2.0
/*
 * openat O_CLOEXEC — musl open() is openat; CLOEXEC is a per-fd bit.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../kselftest.h"

static int fd_has_cloexec(int fd)
{
	int fl = fcntl(fd, F_GETFD);

	return fl >= 0 && (fl & FD_CLOEXEC);
}

static void test_open_plain(void)
{
	int fd;
	int fl;

	fd = open("/msg.txt", O_RDONLY);
	if (fd < 0) {
		ksft_test_result_fail("open /msg.txt\n");
		return;
	}
	fl = fcntl(fd, F_GETFD);
	ksft_test_result(fl >= 0 && !(fl & FD_CLOEXEC),
			 "open without O_CLOEXEC leaves FD_CLOEXEC clear\n");
	close(fd);
}

static void test_open_cloexec(void)
{
	int fd;

	fd = open("/msg.txt", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		ksft_test_result_fail("open O_CLOEXEC\n");
		return;
	}
	ksft_test_result(fd_has_cloexec(fd),
			 "open O_CLOEXEC sets FD_CLOEXEC\n");
	close(fd);
}

static void test_open_chr_cloexec(void)
{
	int fd;

	fd = open("/dev/null", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		ksft_test_result_fail("open /dev/null O_CLOEXEC\n");
		return;
	}
	ksft_test_result(fd_has_cloexec(fd),
			 "open chrdev O_CLOEXEC sets FD_CLOEXEC\n");
	close(fd);
}

static void test_exec_closes(char *self)
{
	int fd;
	pid_t pid;
	int status;
	char nbuf[16];
	char *p;
	unsigned int v;

	fd = open("/msg.txt", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		ksft_test_result_fail("open for exec check\n");
		return;
	}

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		close(fd);
		return;
	}
	if (pid == 0) {
		v = (unsigned int)fd;
		p = nbuf + sizeof(nbuf) - 1;
		*p = '\0';
		if (v == 0)
			*--p = '0';
		while (v) {
			*--p = (char)('0' + (v % 10));
			v /= 10;
		}
		execl(self, self, "--cloexec-check", p, (char *)NULL);
		_exit(127);
	}

	close(fd);
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		ksft_test_result_fail("exec closes O_CLOEXEC fd (status=%d)\n",
				     status);
	else
		ksft_test_result_pass("exec closes O_CLOEXEC fd\n");
}

int main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--cloexec-check") == 0) {
		int fd = atoi(argv[2]);

		if (fcntl(fd, F_GETFD) < 0 && errno == EBADF)
			_exit(0);
		_exit(1);
	}

	ksft_print_header();
	ksft_set_plan(4);

	test_open_plain();
	test_open_cloexec();
	test_open_chr_cloexec();
	test_exec_closes((argc > 0 && argv[0] && argv[0][0] == '/') ?
				 argv[0] :
				 "/kselftests/files/open_test");

	ksft_finished();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * poll(2) via ppoll: files, pipes, POLLNVAL, timeout.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "../kselftest.h"

int main(void)
{
	struct pollfd pfd;
	int fds[2];
	int fd;
	char c;
	int n;

	ksft_print_header();
	ksft_set_plan(6);

	fd = open("/msg.txt", O_RDONLY);
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	ksft_test_result(fd >= 0 && n == 1 && (pfd.revents & POLLIN),
			 "regular file is POLLIN immediately\n");
	if (fd >= 0)
		close(fd);

	pfd.fd = 123456;
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	ksft_test_result(n == 1 && (pfd.revents & POLLNVAL),
			 "bad fd is POLLNVAL\n");

	if (pipe(fds) < 0) {
		ksft_test_result_fail("pipe\n");
		ksft_test_result_fail("empty pipe timeout\n");
		ksft_test_result_fail("pipe POLLIN after write\n");
		ksft_test_result_fail("pipe write POLLOUT\n");
		ksft_test_result_fail("pipe read POLLHUP\n");
		ksft_finished();
	}

	pfd.fd = fds[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	ksft_test_result(n == 0 && pfd.revents == 0,
			 "empty pipe timeout 0 is not ready\n");

	c = 'x';
	write(fds[1], &c, 1);
	pfd.fd = fds[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	ksft_test_result(n == 1 && (pfd.revents & POLLIN),
			 "pipe POLLIN after write\n");

	pfd.fd = fds[1];
	pfd.events = POLLOUT;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	ksft_test_result(n == 1 && (pfd.revents & POLLOUT),
			 "pipe write end is POLLOUT\n");

	close(fds[1]);
	pfd.fd = fds[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	ksft_test_result(n == 1 && (pfd.revents & (POLLIN | POLLHUP)),
			 "reader sees POLLIN/POLLHUP after writer close\n");
	close(fds[0]);

	ksft_finished();
}

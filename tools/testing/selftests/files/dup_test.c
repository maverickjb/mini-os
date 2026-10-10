// SPDX-License-Identifier: GPL-2.0
/*
 * dup / dup2 / dup3 — AArch64 has no dup2 syscall; musl implements
 * dup2() with fcntl(F_GETFD) when old==new and dup3 otherwise.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "../kselftest.h"

static int raw_dup3(int oldfd, int newfd, int flags)
{
	return (int)syscall(SYS_dup3, oldfd, newfd, flags);
}

static void test_dup2_copy(void)
{
	int p[2];
	int d;
	char c = 0;

	if (pipe(p) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	d = dup2(p[1], 20);
	ksft_test_result(d == 20, "dup2 copies write end to fd 20\n");
	if (d != 20) {
		close(p[0]);
		close(p[1]);
		return;
	}

	if (write(20, "Z", 1) != 1 || read(p[0], &c, 1) != 1 || c != 'Z')
		ksft_test_result_fail("write/read via dup2 fd\n");
	else
		ksft_test_result_pass("write/read via dup2 fd\n");

	close(20);
	close(p[0]);
	close(p[1]);
}

static void test_dup2_same_fd(void)
{
	int p[2];
	int r;

	if (pipe(p) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	errno = 0;
	r = dup2(p[0], p[0]);
	ksft_test_result(r == p[0] && errno == 0,
			 "dup2(fd, fd) returns fd\n");

	errno = 0;
	r = dup2(99, 99);
	ksft_test_result(r < 0 && errno == EBADF,
			 "dup2(closed, closed) is EBADF\n");

	close(p[0]);
	close(p[1]);
}

static void test_dup2_replace(void)
{
	int a[2], b[2];
	char c = 0;

	if (pipe(a) < 0 || pipe(b) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	if (dup2(a[1], b[1]) != b[1]) {
		ksft_test_result_fail("dup2 replace\n");
		return;
	}

	if (write(b[1], "Q", 1) != 1)
		ksft_test_result_fail("write after replace\n");
	else if (read(a[0], &c, 1) != 1 || c != 'Q')
		ksft_test_result_fail("replaced fd writes into old pipe\n");
	else
		ksft_test_result_pass("dup2 replaces target fd\n");

	close(a[0]);
	close(a[1]);
	close(b[0]);
	close(b[1]);
}

static void test_dup3_same_fd(void)
{
	int p[2];
	int r;

	if (pipe(p) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	errno = 0;
	r = raw_dup3(p[0], p[0], 0);
	ksft_test_result(r < 0 && errno == EINVAL,
			 "dup3(fd, fd) is EINVAL\n");

	close(p[0]);
	close(p[1]);
}

static void test_dup2_clears_cloexec(void)
{
	int p[2];
	int d;
	int flags;

	if (pipe(p) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	d = raw_dup3(p[0], 21, O_CLOEXEC);
	if (d != 21) {
		ksft_test_result_fail("dup3 O_CLOEXEC\n");
		close(p[0]);
		close(p[1]);
		return;
	}
	flags = fcntl(21, F_GETFD);
	ksft_test_result(flags >= 0 && (flags & FD_CLOEXEC),
			 "dup3 sets FD_CLOEXEC\n");

	if (dup2(p[0], 21) != 21)
		ksft_test_result_fail("dup2 onto cloexec fd\n");
	else {
		flags = fcntl(21, F_GETFD);
		ksft_test_result(flags >= 0 && !(flags & FD_CLOEXEC),
				 "dup2 clears FD_CLOEXEC\n");
	}

	close(21);
	close(p[0]);
	close(p[1]);
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(8);

	test_dup2_copy();
	test_dup2_same_fd();
	test_dup2_replace();
	test_dup3_same_fd();
	test_dup2_clears_cloexec();

	ksft_finished();
}

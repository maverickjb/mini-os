// SPDX-License-Identifier: GPL-2.0
/*
 * pipe2 flags and non-blocking pipe I/O (O_CLOEXEC, O_NONBLOCK, F_SETFL).
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "../kselftest.h"

static int raw_pipe2(int fds[2], int flags)
{
	return (int)syscall(SYS_pipe2, fds, flags);
}

static void close_pipe(int p[2])
{
	close(p[0]);
	close(p[1]);
}

static void test_bad_flags(void)
{
	int p[2];

	errno = 0;
	ksft_test_result(raw_pipe2(p, 0x1234) < 0 && errno == EINVAL,
			 "pipe2 rejects unknown flags\n");
}

static void test_cloexec(void)
{
	int p[2];
	int f0, f1;

	if (raw_pipe2(p, O_CLOEXEC) < 0) {
		ksft_test_result_fail("pipe2 O_CLOEXEC\n");
		return;
	}

	f0 = fcntl(p[0], F_GETFD);
	f1 = fcntl(p[1], F_GETFD);
	ksft_test_result(f0 >= 0 && f1 >= 0 &&
			 (f0 & FD_CLOEXEC) && (f1 & FD_CLOEXEC),
			 "pipe2 O_CLOEXEC sets FD_CLOEXEC on both ends\n");
	close_pipe(p);
}

static void test_nonblock_empty_read(void)
{
	int p[2];
	char c;
	ssize_t n;

	if (raw_pipe2(p, O_NONBLOCK) < 0) {
		ksft_test_result_fail("pipe2 O_NONBLOCK\n");
		return;
	}

	errno = 0;
	n = read(p[0], &c, 1);
	ksft_test_result(n < 0 && errno == EAGAIN,
			 "non-blocking empty read is EAGAIN\n");
	close_pipe(p);
}

static void test_nonblock_full_write(void)
{
	int p[2];
	char buf[256];
	ssize_t n;
	int saw_eagain = 0;
	int i;

	if (raw_pipe2(p, O_NONBLOCK) < 0) {
		ksft_test_result_fail("pipe2 O_NONBLOCK\n");
		return;
	}

	for (i = 0; i < 32; i++) {
		n = write(p[1], buf, sizeof(buf));
		if (n < 0) {
			if (errno == EAGAIN)
				saw_eagain = 1;
			break;
		}
	}

	ksft_test_result(saw_eagain, "non-blocking full write is EAGAIN\n");
	close_pipe(p);
}

static void test_nonblock_data_and_eof(void)
{
	int p[2];
	char c = 0;
	ssize_t n;

	if (raw_pipe2(p, O_NONBLOCK) < 0) {
		ksft_test_result_fail("pipe2 O_NONBLOCK\n");
		return;
	}

	if (write(p[1], "A", 1) != 1) {
		ksft_test_result_fail("non-blocking write with room\n");
		close_pipe(p);
		return;
	}

	n = read(p[0], &c, 1);
	ksft_test_result(n == 1 && c == 'A',
			 "non-blocking read returns data when present\n");

	close(p[1]);
	n = read(p[0], &c, 1);
	ksft_test_result(n == 0, "non-blocking read is EOF with no writers\n");
	close(p[0]);
}

static void test_fcntl_setfl(void)
{
	int p[2];
	int fl;
	char c;
	ssize_t n;

	if (pipe(p) < 0) {
		ksft_test_result_fail("pipe\n");
		return;
	}

	fl = fcntl(p[0], F_GETFL);
	if (fl < 0 || fcntl(p[0], F_SETFL, fl | O_NONBLOCK) < 0) {
		ksft_test_result_fail("F_SETFL O_NONBLOCK\n");
		close_pipe(p);
		return;
	}

	errno = 0;
	n = read(p[0], &c, 1);
	ksft_test_result(n < 0 && errno == EAGAIN,
			 "F_SETFL O_NONBLOCK makes empty read EAGAIN\n");
	close_pipe(p);
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(7);

	test_bad_flags();
	test_cloexec();
	test_nonblock_empty_read();
	test_nonblock_full_write();
	test_nonblock_data_and_eof();
	test_fcntl_setfl();

	ksft_finished();
}

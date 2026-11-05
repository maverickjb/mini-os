// SPDX-License-Identifier: GPL-2.0
/*
 * faccessat(2) / access(2) — F_OK, R_OK, W_OK, X_OK.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "../kselftest.h"

static int raw_faccessat2(int dfd, const char *path, int mode, int flags)
{
	return (int)syscall(SYS_faccessat2, dfd, path, mode, flags);
}

static void test_exists(void)
{
	errno = 0;
	ksft_test_result(access("/msg.txt", F_OK) == 0,
			 "access F_OK existing file\n");
	errno = 0;
	ksft_test_result(access("/no-such-file", F_OK) < 0 && errno == ENOENT,
			 "access F_OK missing file is ENOENT\n");
}

static void test_rwx(void)
{
	ksft_test_result(access("/msg.txt", R_OK) == 0,
			 "access R_OK regular file\n");
	ksft_test_result(access("/msg.txt", W_OK) == 0,
			 "access W_OK regular file\n");
	ksft_test_result(access("/bin/busybox", X_OK) == 0,
			 "access X_OK executable\n");
	ksft_test_result(access("/tmp", R_OK | W_OK | X_OK) == 0,
			 "access RWX on directory\n");
}

static void test_chr_nox(void)
{
	errno = 0;
	ksft_test_result(access("/dev/null", R_OK | W_OK) == 0,
			 "access RW on /dev/null\n");
	errno = 0;
	ksft_test_result(access("/dev/null", X_OK) < 0 && errno == EACCES,
			 "access X_OK on /dev/null is EACCES\n");
}

static void test_bad_mode(void)
{
	errno = 0;
	ksft_test_result(access("/msg.txt", 0x8) < 0 && errno == EINVAL,
			 "access rejects unknown mode bits\n");
}

static void test_faccessat2_flags(void)
{
	int fd;

	errno = 0;
	ksft_test_result(raw_faccessat2(AT_FDCWD, "/msg.txt", F_OK, 0) == 0,
			 "faccessat2 F_OK\n");
	errno = 0;
	ksft_test_result(raw_faccessat2(AT_FDCWD, "/msg.txt", F_OK, 0x4) < 0 &&
			 errno == EINVAL,
			 "faccessat2 rejects unknown flags\n");

	fd = open("/msg.txt", O_RDONLY);
	if (fd < 0) {
		ksft_test_result_fail("open for AT_EMPTY_PATH\n");
		return;
	}
	errno = 0;
	ksft_test_result(raw_faccessat2(fd, "", F_OK, AT_EMPTY_PATH) == 0,
			 "faccessat2 AT_EMPTY_PATH\n");
	close(fd);
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(12);

	test_exists();
	test_rwx();
	test_chr_nox();
	test_bad_mode();
	test_faccessat2_flags();

	ksft_finished();
}

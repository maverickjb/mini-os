// SPDX-License-Identifier: GPL-2.0
/*
 * Relative *at paths start at dfd (AT_FDCWD = cwd; absolute names ignore dfd).
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../kselftest.h"

static void cleanup(void)
{
	unlink("/tmp/atd/f");
	rmdir("/tmp/atd/sub");
	rmdir("/tmp/atd");
}

int main(void)
{
	int dirfd;
	int fd;
	int filefd;

	ksft_print_header();
	ksft_set_plan(11);

	cleanup();
	if (mkdir("/tmp/atd", 0755) < 0) {
		ksft_finished();
		return 1;
	}

	dirfd = open("/tmp/atd", O_RDONLY | O_DIRECTORY);
	if (dirfd < 0) {
		ksft_test_result_fail("open /tmp/atd\n");
		ksft_finished();
		return 1;
	}

	fd = openat(dirfd, "f", O_CREAT | O_WRONLY, 0644);
	ksft_test_result(fd >= 0, "openat(dirfd, \"f\") creates in that dir\n");
	if (fd >= 0)
		close(fd);

	ksft_test_result(faccessat(dirfd, "f", F_OK, 0) == 0,
			 "faccessat(dirfd, \"f\") sees the new file\n");

	errno = 0;
	ksft_test_result(faccessat(dirfd, "missing", F_OK, 0) < 0 &&
			 errno == ENOENT,
			 "faccessat relative missing is ENOENT\n");

	fd = openat(dirfd, "/msg.txt", O_RDONLY);
	ksft_test_result(fd >= 0, "openat absolute path ignores dfd\n");
	if (fd >= 0)
		close(fd);

	errno = 0;
	ksft_test_result(openat(99, "f", O_RDONLY) < 0 && errno == EBADF,
			 "openat bad dfd is EBADF\n");

	filefd = open("/msg.txt", O_RDONLY);
	if (filefd < 0) {
		ksft_test_result_fail("open /msg.txt\n");
	} else {
		errno = 0;
		ksft_test_result(openat(filefd, "f", O_RDONLY) < 0 &&
				 errno == ENOTDIR,
				 "openat on non-dir fd is ENOTDIR\n");
		close(filefd);
	}

	ksft_test_result(mkdirat(dirfd, "sub", 0755) == 0 &&
			 faccessat(dirfd, "sub", X_OK, 0) == 0,
			 "mkdirat + faccessat relative to dfd\n");

	{
		char cwd[64];

		ksft_test_result(fchdir(dirfd) == 0 &&
				 getcwd(cwd, sizeof(cwd)) &&
				 strcmp(cwd, "/tmp/atd") == 0,
				 "fchdir + getcwd\n");
		close(dirfd);
		ksft_test_result(getcwd(cwd, sizeof(cwd)) &&
				 strcmp(cwd, "/tmp/atd") == 0,
				 "cwd survives closing the dir fd\n");
		chdir("/");
	}

	filefd = open("/msg.txt", O_RDONLY);
	if (filefd < 0) {
		ksft_test_result_fail("open /msg.txt for fchdir\n");
	} else {
		errno = 0;
		ksft_test_result(fchdir(filefd) < 0 && errno == ENOTDIR,
				 "fchdir on non-dir is ENOTDIR\n");
		close(filefd);
	}

	errno = 0;
	ksft_test_result(fchdir(99) < 0 && errno == EBADF,
			 "fchdir bad fd is EBADF\n");

	cleanup();
	ksft_finished();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * fchmod(2) / fchmodat(2) / chmod(2) — stored inode permission bits.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../kselftest.h"

static void cleanup(void)
{
	unlink("/tmp/chmodf");
	rmdir("/tmp/chmodd");
}

int main(void)
{
	struct stat st;
	int fd;
	int dirfd;

	ksft_print_header();
	ksft_set_plan(7);

	cleanup();

	fd = open("/tmp/chmodf", O_RDWR | O_CREAT, 0644);
	if (fd < 0) {
		ksft_test_result_fail("create /tmp/chmodf\n");
		ksft_finished();
	}

	ksft_test_result(fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0644,
			 "O_CREAT mode 0644\n");

	ksft_test_result(fchmod(fd, 0000) == 0 &&
			 fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0000 &&
			 access("/tmp/chmodf", W_OK) < 0 && errno == EACCES,
			 "fchmod 000 then access W_OK is EACCES\n");

	errno = 0;
	ksft_test_result(chmod("/tmp/chmodf", 0644) == 0 &&
			 access("/tmp/chmodf", R_OK | W_OK) == 0 &&
			 fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0644,
			 "chmod 0644 restores RW\n");

	ksft_test_result(fchmod(fd, 0700) == 0 &&
			 fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0700,
			 "fchmod 0700\n");
	close(fd);

	if (mkdir("/tmp/chmodd", 0755) < 0) {
		ksft_test_result_fail("mkdir /tmp/chmodd\n");
	} else {
		dirfd = open("/tmp/chmodd", O_RDONLY | O_DIRECTORY);
		if (dirfd < 0) {
			ksft_test_result_fail("open /tmp/chmodd\n");
		} else {
			ksft_test_result(fchmodat(dirfd, "../chmodf", 0600, 0) == 0 &&
					 stat("/tmp/chmodf", &st) == 0 &&
					 (st.st_mode & 0777) == 0600,
					 "fchmodat relative to dirfd\n");
			close(dirfd);
		}
	}

	errno = 0;
	ksft_test_result(fchmod(99, 0644) < 0 && errno == EBADF,
			 "fchmod bad fd is EBADF\n");

	errno = 0;
	ksft_test_result(chmod("/no-such-chmod", 0644) < 0 && errno == ENOENT,
			 "chmod missing path is ENOENT\n");

	cleanup();
	ksft_finished();
}

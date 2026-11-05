// SPDX-License-Identifier: GPL-2.0
/*
 * Inode i_count: unlink of an open file must not destroy the inode until
 * the last close.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../kselftest.h"

static void test_unlink_open(void)
{
	int fd;
	char c = 0;
	struct stat st;

	unlink("/tmp/orphan");
	fd = open("/tmp/orphan", O_CREAT | O_RDWR, 0644);
	if (fd < 0) {
		ksft_test_result_fail("create /tmp/orphan\n");
		return;
	}
	if (write(fd, "Q", 1) != 1) {
		ksft_test_result_fail("write before unlink\n");
		close(fd);
		return;
	}
	if (unlink("/tmp/orphan") < 0) {
		ksft_test_result_fail("unlink open file\n");
		close(fd);
		return;
	}

	ksft_test_result(stat("/tmp/orphan", &st) < 0 && errno == ENOENT,
			 "unlinked name is gone\n");

	if (lseek(fd, 0, SEEK_SET) < 0 || read(fd, &c, 1) != 1 || c != 'Q')
		ksft_test_result_fail("read after unlink\n");
	else
		ksft_test_result_pass("open fd still reads after unlink\n");

	close(fd);
	ksft_test_result(stat("/tmp/orphan", &st) < 0 && errno == ENOENT,
			 "name still gone after close\n");
}

static void test_proc_access(void)
{
	ksft_test_result(access("/proc/1/stat", R_OK) == 0,
			 "access /proc/1/stat (iget/iput proc inode)\n");
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(4);

	test_unlink_open();
	test_proc_access();

	ksft_finished();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * renameat(2) — musl rename() is renameat(AT_FDCWD, old, AT_FDCWD, new).
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../kselftest.h"

static int write_file(const char *path, const char *s)
{
	int fd;
	ssize_t n;
	size_t len;

	fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
		return -1;
	len = 0;
	while (s[len])
		len++;
	n = write(fd, s, len);
	close(fd);
	return n == (ssize_t)len ? 0 : -1;
}

static int read_one(const char *path, char *c)
{
	int fd;
	ssize_t n;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	n = read(fd, c, 1);
	close(fd);
	return n == 1 ? 0 : -1;
}

static void test_rename_file(void)
{
	char c = 0;
	struct stat st;

	unlink("/tmp/rn_a");
	unlink("/tmp/rn_b");
	if (write_file("/tmp/rn_a", "X") < 0) {
		ksft_test_result_fail("create /tmp/rn_a\n");
		return;
	}
	if (rename("/tmp/rn_a", "/tmp/rn_b") < 0) {
		ksft_test_result_fail("rename file\n");
		return;
	}
	ksft_test_result(stat("/tmp/rn_a", &st) < 0 && errno == ENOENT,
			 "rename removes old name\n");
	ksft_test_result(read_one("/tmp/rn_b", &c) == 0 && c == 'X',
			 "rename keeps file contents\n");
	unlink("/tmp/rn_b");
}

static void test_rename_into_dir(void)
{
	struct stat st;

	unlink("/tmp/rn_c");
	rmdir("/tmp/rn_d");
	if (write_file("/tmp/rn_c", "Y") < 0 || mkdir("/tmp/rn_d", 0755) < 0) {
		ksft_test_result_fail("setup rename into dir\n");
		return;
	}
	if (rename("/tmp/rn_c", "/tmp/rn_d/z") < 0)
		ksft_test_result_fail("rename into directory\n");
	else
		ksft_test_result(stat("/tmp/rn_d/z", &st) == 0,
				 "rename into directory\n");
	unlink("/tmp/rn_d/z");
	rmdir("/tmp/rn_d");
}

static void test_replace_file(void)
{
	char c = 0;

	unlink("/tmp/rn_old");
	unlink("/tmp/rn_new");
	if (write_file("/tmp/rn_old", "A") < 0 ||
	    write_file("/tmp/rn_new", "B") < 0) {
		ksft_test_result_fail("setup replace file\n");
		return;
	}
	if (rename("/tmp/rn_old", "/tmp/rn_new") < 0)
		ksft_test_result_fail("replace dest file\n");
	else
		ksft_test_result(read_one("/tmp/rn_new", &c) == 0 && c == 'A',
				 "rename replaces dest file\n");
	unlink("/tmp/rn_new");
}

static void test_replace_empty_dir(void)
{
	struct stat st;

	rmdir("/tmp/rn_srcd");
	rmdir("/tmp/rn_dstd");
	if (mkdir("/tmp/rn_srcd", 0755) < 0 || mkdir("/tmp/rn_dstd", 0755) < 0) {
		ksft_test_result_fail("setup replace dir\n");
		return;
	}
	if (rename("/tmp/rn_srcd", "/tmp/rn_dstd") < 0)
		ksft_test_result_fail("replace empty dir\n");
	else
		ksft_test_result(stat("/tmp/rn_dstd", &st) == 0 &&
				 stat("/tmp/rn_srcd", &st) < 0,
				 "rename replaces empty directory\n");
	rmdir("/tmp/rn_dstd");
}

static void test_type_mismatch(void)
{
	rmdir("/tmp/rn_dir");
	unlink("/tmp/rn_file");
	if (mkdir("/tmp/rn_dir", 0755) < 0 ||
	    write_file("/tmp/rn_file", "Z") < 0) {
		ksft_test_result_fail("setup type mismatch\n");
		return;
	}

	errno = 0;
	ksft_test_result(rename("/tmp/rn_file", "/tmp/rn_dir") < 0 &&
			 errno == EISDIR,
			 "file over directory is EISDIR\n");
	errno = 0;
	ksft_test_result(rename("/tmp/rn_dir", "/tmp/rn_file") < 0 &&
			 errno == ENOTDIR,
			 "directory over file is ENOTDIR\n");

	unlink("/tmp/rn_file");
	rmdir("/tmp/rn_dir");
}

static void test_notempty_and_into_self(void)
{
	rmdir("/tmp/rn_p/child");
	rmdir("/tmp/rn_p");
	rmdir("/tmp/rn_q");
	if (mkdir("/tmp/rn_p", 0755) < 0 || mkdir("/tmp/rn_p/child", 0755) < 0 ||
	    mkdir("/tmp/rn_q", 0755) < 0) {
		ksft_test_result_fail("setup notempty\n");
		return;
	}

	errno = 0;
	ksft_test_result(rename("/tmp/rn_q", "/tmp/rn_p") < 0 &&
			 errno == ENOTEMPTY,
			 "rename over non-empty dir is ENOTEMPTY\n");
	errno = 0;
	ksft_test_result(rename("/tmp/rn_p", "/tmp/rn_p/child/x") < 0 &&
			 errno == EINVAL,
			 "rename dir into descendant is EINVAL\n");

	rmdir("/tmp/rn_p/child");
	rmdir("/tmp/rn_p");
	rmdir("/tmp/rn_q");
}

static void test_same_path(void)
{
	struct stat st;

	unlink("/tmp/rn_same");
	if (write_file("/tmp/rn_same", "S") < 0) {
		ksft_test_result_fail("create same-path file\n");
		return;
	}
	ksft_test_result(rename("/tmp/rn_same", "/tmp/rn_same") == 0 &&
			 stat("/tmp/rn_same", &st) == 0,
			 "rename to same path succeeds\n");
	unlink("/tmp/rn_same");
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(10);

	test_rename_file();
	test_rename_into_dir();
	test_replace_file();
	test_replace_empty_dir();
	test_type_mismatch();
	test_notempty_and_into_self();
	test_same_path();

	ksft_finished();
}

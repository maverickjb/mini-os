// SPDX-License-Identifier: GPL-2.0
/*
 * mprotect: VMA prot, holes, split, and COW-safe PTE updates.
 */

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../kselftest.h"

#define PAGE_SIZE 4096UL

static int child_write_dies(char *p)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		p[0] = 0x55;
		_exit(0);
	}
	if (waitpid(pid, &status, 0) != pid)
		return -1;
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV;
}

static void test_args(void)
{
	char *p;

	p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		ksft_test_result_fail("mmap\n");
		return;
	}

	/* musl aligns before the syscall; hit the kernel check directly. */
	ksft_test_result(syscall(SYS_mprotect, p + 1, PAGE_SIZE, PROT_READ) < 0 &&
				 errno == EINVAL,
			 "unaligned addr is EINVAL\n");
	ksft_test_result(mprotect(p, PAGE_SIZE, 0x10) < 0 && errno == EINVAL,
			 "unknown prot bit is EINVAL\n");
	ksft_test_result(mprotect(p, 0, PROT_READ) == 0, "len 0 succeeds\n");

	munmap(p, PAGE_SIZE);
}

static void test_prot_none_write_faults(void)
{
	char *p;

	p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		ksft_test_result_fail("mmap\n");
		return;
	}
	p[0] = 0x33;
	if (mprotect(p, PAGE_SIZE, PROT_READ) != 0) {
		ksft_test_result_fail("mprotect PROT_READ\n");
		munmap(p, PAGE_SIZE);
		return;
	}

	ksft_test_result(child_write_dies(p) == 1,
			 "write after PROT_READ is SIGSEGV\n");
	ksft_test_result(p[0] == 0x33, "parent still sees old byte\n");

	munmap(p, PAGE_SIZE);
}

static void test_hole(void)
{
	char *p;
	int r;

	p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		ksft_test_result_fail("mmap\n");
		return;
	}

	r = mprotect(p, PAGE_SIZE * 2, PROT_READ);
	ksft_test_result(r < 0 && errno == ENOMEM,
			 "range with a hole is ENOMEM\n");
	/* First page must still be writable — no partial update. */
	p[0] = 0x22;
	ksft_test_result(p[0] == 0x22, "hole failure does not strip write\n");

	munmap(p, PAGE_SIZE);
}

static void test_split(void)
{
	char *p;

	p = mmap(NULL, PAGE_SIZE * 3, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		ksft_test_result_fail("mmap\n");
		return;
	}
	memset(p, 0x11, PAGE_SIZE * 3);

	if (mprotect(p + PAGE_SIZE, PAGE_SIZE, PROT_READ) != 0) {
		ksft_test_result_fail("mprotect middle\n");
		munmap(p, PAGE_SIZE * 3);
		return;
	}

	p[0] = 0x21;
	p[PAGE_SIZE * 2] = 0x23;
	ksft_test_result(p[0] == 0x21 && p[PAGE_SIZE * 2] == 0x23,
			 "edges stay writable after split\n");
	ksft_test_result(child_write_dies(p + PAGE_SIZE) == 1,
			 "middle page write is SIGSEGV\n");

	munmap(p, PAGE_SIZE * 3);
}

static void test_cow_stays_ro(void)
{
	char *p;
	int go[2], done[2];
	pid_t pid;
	int status;
	char b;

	p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		ksft_test_result_fail("mmap\n");
		return;
	}
	memset(p, 0x33, PAGE_SIZE);

	if (pipe(go) < 0 || pipe(done) < 0) {
		ksft_test_result_fail("pipe\n");
		munmap(p, PAGE_SIZE);
		return;
	}

	pid = fork();
	if (pid < 0) {
		ksft_test_result_fail("fork\n");
		munmap(p, PAGE_SIZE);
		return;
	}

	if (pid == 0) {
		if (read(go[0], &b, 1) != 1)
			_exit(1);
		/* Re-affirm RW; shared COW page must stay hardware RO. */
		if (mprotect(p, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0)
			_exit(2);
		p[0] = 0x44;
		if (write(done[1], "0", 1) != 1)
			_exit(3);
		_exit(0);
	}

	if (write(go[1], "0", 1) != 1 || read(done[0], &b, 1) != 1) {
		ksft_test_result_fail("sync\n");
		(void)waitpid(pid, &status, 0);
		munmap(p, PAGE_SIZE);
		return;
	}

	ksft_test_result(p[0] == 0x33,
			 "mprotect RW on COW page does not break isolation\n");

	(void)waitpid(pid, &status, 0);
	munmap(p, PAGE_SIZE);
}

int main(void)
{
	ksft_print_header();
	ksft_set_plan(10);

	test_args();
	test_prot_none_write_faults();
	test_hole();
	test_split();
	test_cow_stays_ro();

	ksft_finished();
}

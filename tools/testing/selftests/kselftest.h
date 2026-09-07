/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Minimal kselftest.h for mini-os — API shaped like Linux
 * tools/testing/selftests/kselftest.h (TAP reporting).
 *
 * Usage:
 *   ksft_print_header();
 *   ksft_set_plan(n);
 *   ksft_test_result(cond, "name\n");
 *   ksft_finished();
 */
#ifndef __KSELFTEST_H
#define __KSELFTEST_H

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define KSFT_PASS  0
#define KSFT_FAIL  1
#define KSFT_XFAIL 2
#define KSFT_XPASS 3
#define KSFT_SKIP  4

#ifndef __noreturn
#define __noreturn __attribute__((__noreturn__))
#endif
#define __printf(a, b) __attribute__((format(printf, a, b)))

struct ksft_count {
	unsigned int ksft_pass;
	unsigned int ksft_fail;
	unsigned int ksft_xfail;
	unsigned int ksft_xpass;
	unsigned int ksft_xskip;
	unsigned int ksft_error;
};

static struct ksft_count ksft_cnt;
static unsigned int ksft_plan;

static inline unsigned int ksft_test_num(void)
{
	return ksft_cnt.ksft_pass + ksft_cnt.ksft_fail +
	       ksft_cnt.ksft_xfail + ksft_cnt.ksft_xpass +
	       ksft_cnt.ksft_xskip + ksft_cnt.ksft_error;
}

static inline void ksft_print_header(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	if (!getenv("KSFT_TAP_LEVEL"))
		printf("TAP version 13\n");
}

static inline void ksft_set_plan(unsigned int plan)
{
	ksft_plan = plan;
	printf("1..%u\n", ksft_plan);
}

static inline void ksft_print_cnts(void)
{
	if (ksft_plan != ksft_test_num())
		printf("# Planned tests != run tests (%u != %u)\n",
		       ksft_plan, ksft_test_num());
	printf("# Totals: pass:%u fail:%u xfail:%u xpass:%u skip:%u error:%u\n",
	       ksft_cnt.ksft_pass, ksft_cnt.ksft_fail,
	       ksft_cnt.ksft_xfail, ksft_cnt.ksft_xpass,
	       ksft_cnt.ksft_xskip, ksft_cnt.ksft_error);
}

static inline __printf(1, 2) void ksft_print_msg(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	va_start(args, msg);
	printf("# ");
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline void ksft_perror(const char *msg)
{
	ksft_print_msg("%s: %s (%d)\n", msg, strerror(errno), errno);
}

static inline __printf(1, 2) void ksft_test_result_pass(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_pass++;
	va_start(args, msg);
	printf("ok %u ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline __printf(1, 2) void ksft_test_result_fail(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_fail++;
	va_start(args, msg);
	printf("not ok %u ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

#define ksft_test_result(condition, fmt, ...) do {		\
	if (!!(condition))					\
		ksft_test_result_pass(fmt, ##__VA_ARGS__);	\
	else							\
		ksft_test_result_fail(fmt, ##__VA_ARGS__);	\
} while (0)

static inline __printf(1, 2) void ksft_test_result_skip(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_xskip++;
	va_start(args, msg);
	printf("ok %u # SKIP ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline __noreturn void ksft_exit_pass(void)
{
	ksft_print_cnts();
	exit(KSFT_PASS);
}

static inline __noreturn void ksft_exit_fail(void)
{
	ksft_print_cnts();
	exit(KSFT_FAIL);
}

#define ksft_exit(condition) do {	\
	if (!!(condition))		\
		ksft_exit_pass();	\
	else				\
		ksft_exit_fail();	\
} while (0)

#define ksft_finished()						\
	ksft_exit(ksft_plan ==					\
		  ksft_cnt.ksft_pass +				\
		  ksft_cnt.ksft_xpass +				\
		  ksft_cnt.ksft_xfail +				\
		  ksft_cnt.ksft_xskip)

static inline __noreturn __printf(1, 2)
void ksft_exit_fail_msg(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	va_start(args, msg);
	printf("Bail out! ");
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
	ksft_print_cnts();
	exit(KSFT_FAIL);
}

#endif /* __KSELFTEST_H */

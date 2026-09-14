#ifndef __LINUX_ATOMIC_H
#define __LINUX_ATOMIC_H

/*
 * Minimal atomic_t for freestanding AArch64 (no libatomic).
 * LDAXR / STLXR — enough for page->_refcount on SMP.
 */

typedef struct {
	volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i) { (i) }

static inline void atomic_set(atomic_t *v, int i)
{
	__asm__ volatile(
		"stlr %w1, [%0]"
		:
		: "r"(&v->counter), "r"(i)
		: "memory");
}

static inline int atomic_read(const atomic_t *v)
{
	int val;

	__asm__ volatile("ldar %w0, [%1]"
			 : "=r"(val)
			 : "r"(&v->counter)
			 : "memory");
	return val;
}

static inline void atomic_inc(atomic_t *v)
{
	unsigned int tmp;
	int result;

	__asm__ volatile(
		"1:	ldaxr	%w0, [%2]\n"
		"	add	%w0, %w0, #1\n"
		"	stlxr	%w1, %w0, [%2]\n"
		"	cbnz	%w1, 1b\n"
		: "=&r"(result), "=&r"(tmp)
		: "r"(&v->counter)
		: "memory");
}

/* Returns true if the new value is 0. */
static inline int atomic_dec_and_test(atomic_t *v)
{
	unsigned int tmp;
	int result;

	__asm__ volatile(
		"1:	ldaxr	%w0, [%2]\n"
		"	sub	%w0, %w0, #1\n"
		"	stlxr	%w1, %w0, [%2]\n"
		"	cbnz	%w1, 1b\n"
		: "=&r"(result), "=&r"(tmp)
		: "r"(&v->counter)
		: "memory");

	return result == 0;
}

#endif /* __LINUX_ATOMIC_H */

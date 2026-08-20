#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#define WNOHANG		0x00000001

/*
 * Linux wait status: low 7 bits are the terminating signal (0 = exited).
 * Exit code is bits 8–15.
 */
#define WIFEXITED(status)	(((status) & 0x7f) == 0)
#define WEXITSTATUS(status)	(((status) >> 8) & 0xff)
#define WIFSIGNALED(status)	(((status) & 0x7f) > 0)
#define WTERMSIG(status)	((status) & 0x7f)

#endif /* _SYS_WAIT_H */

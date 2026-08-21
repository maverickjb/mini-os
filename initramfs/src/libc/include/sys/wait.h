#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#define WNOHANG		0x00000001
#define WUNTRACED	0x00000002
#define WCONTINUED	0x00000008

/*
 * Linux wait status:
 *   exited:     (code << 8)
 *   signaled:   sig in low 7 bits
 *   stopped:    (sig << 8) | 0x7f
 *   continued:  0xffff
 */
#define WIFEXITED(status)	(((status) & 0x7f) == 0)
#define WEXITSTATUS(status)	(((status) >> 8) & 0xff)
#define WIFSIGNALED(status)	(((signed char)(((status) & 0x7f) + 1) >> 1) > 0)
#define WTERMSIG(status)	((status) & 0x7f)
#define WIFSTOPPED(status)	(((status) & 0xff) == 0x7f)
#define WSTOPSIG(status)	(((status) >> 8) & 0xff)
#define WIFCONTINUED(status)	((status) == 0xffff)

#endif /* _SYS_WAIT_H */

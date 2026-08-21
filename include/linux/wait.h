#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#define WNOHANG		0x00000001
#define WUNTRACED	0x00000002
#define WCONTINUED	0x00000008

/* Linux wait words for stop / continue (not a zombie). */
#define W_STOPCODE(sig)	(((sig) << 8) | 0x7f)
#define W_CONTINUED	0xffff

#endif /* _LINUX_WAIT_H */

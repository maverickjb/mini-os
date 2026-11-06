#ifndef _LINUX_POLL_H
#define _LINUX_POLL_H

#include <linux/wait.h>
#include <linux/sched.h>

#define POLLIN          0x0001
#define POLLPRI         0x0002
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLHUP         0x0010
#define POLLNVAL        0x0020
#define POLLRDNORM      0x0040
#define POLLRDBAND      0x0080
#define POLLWRNORM      0x0100
#define POLLWRBAND      0x0200

struct pollfd {
    int fd;
    short events;
    short revents;
};

struct poll_waiter {
    struct wait_queue_entry wait;
    struct wait_queue_head *head;
};

struct poll_table {
    struct poll_waiter waiters[NR_OPEN];
    unsigned int n;
};

void poll_table_init(struct poll_table *pt);
void poll_wait(struct wait_queue_head *wq, struct poll_table *pt);
void poll_table_finish(struct poll_table *pt);

#endif /* _LINUX_POLL_H */

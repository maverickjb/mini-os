#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

int main(void)
{
    char buf[512];
    int fd;
    long n;
    long bpos;

    fd = open("/", O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        printf("open / failed: %d\n", fd);
        return 1;
    }

    printf("getdents64 /:\n");
    for (;;) {
        n = getdents64(fd, buf, sizeof(buf));
        if (n < 0) {
            printf("getdents64 failed: %d\n", (int)n);
            close(fd);
            return 1;
        }
        if (n == 0)
            break;

        for (bpos = 0; bpos < n;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);

            printf("  %s type=%d ino=%d\n",
                   d->d_name, (int)d->d_type, (int)d->d_ino);
            bpos += d->d_reclen;
        }
    }

    close(fd);
    printf("getdents64 ok\n");
    return 0;
}

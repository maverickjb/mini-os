#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

int main(void)
{
    struct stat st;
    char buf[512];
    int fd;
    long n;
    long bpos;
    int ret;

    ret = mkdir("/testdir", 0755);
    if (ret < 0) {
        printf("mkdir /testdir failed: %d\n", ret);
        return 1;
    }

    ret = stat("/testdir", &st);
    if (ret < 0 || !S_ISDIR(st.st_mode)) {
        printf("stat /testdir failed: %d\n", ret);
        return 1;
    }
    printf("mkdir /testdir ok ino=%d\n", (int)st.st_ino);

    ret = mkdir("/testdir", 0755);
    if (ret != -EEXIST) {
        printf("mkdir again expected %d got %d\n", -EEXIST, ret);
        return 1;
    }
    printf("mkdir EEXIST ok\n");

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

            printf("  %s type=%d\n", d->d_name, (int)d->d_type);
            bpos += d->d_reclen;
        }
    }

    close(fd);
    printf("mkdirat ok\n");
    return 0;
}

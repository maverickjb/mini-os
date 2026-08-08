#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

static void print_stat(const char *label, const struct stat *st)
{
    printf("%s: ino=%d mode=0x%x size=%d%s\n",
           label,
           (int)st->st_ino,
           (int)st->st_mode,
           (int)st->st_size,
           S_ISREG(st->st_mode) ? " reg" :
           S_ISDIR(st->st_mode) ? " dir" :
           S_ISCHR(st->st_mode) ? " chr" : "");
}

int main(void)
{
    struct stat st;
    int fd;
    int ret;

    ret = stat("/msg.txt", &st);
    if (ret < 0) {
        printf("stat /msg.txt failed: %d\n", ret);
        return 1;
    }
    print_stat("stat /msg.txt", &st);

    fd = open("/msg.txt", O_RDONLY);
    if (fd < 0) {
        printf("open /msg.txt failed: %d\n", fd);
        return 1;
    }

    ret = fstat(fd, &st);
    if (ret < 0) {
        printf("fstat failed: %d\n", ret);
        close(fd);
        return 1;
    }
    print_stat("fstat msg.txt", &st);

    ret = fstat(STDOUT_FILENO, &st);
    if (ret < 0) {
        printf("fstat stdout failed: %d\n", ret);
        close(fd);
        return 1;
    }
    print_stat("fstat stdout", &st);

    close(fd);
    printf("stat tests ok\n");
    return 0;
}

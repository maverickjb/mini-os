#include <unistd.h>

int main(void)
{
    char buf[128];

    while (1) {
        long n = read(0, buf, sizeof(buf));

        if (n < 0)
            return 1;

        write(1, buf, n);
    }
}

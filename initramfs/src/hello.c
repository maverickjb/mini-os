#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *p;

    printf("hello world!\n");
    printf("printf: %s %d %c\n", "ok", 42, '!');

    p = malloc(32);
    if (p) {
        sprintf(p, "malloc %d", 32);
        printf("%s\n", p);
        free(p);
    }

    return 0;
}

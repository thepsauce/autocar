#include <stdio.h>

int main(void)
{
    int c;

    printf("hello there, I am output\n");
    while (c = fgetc(stdin), c != EOF) {
        putchar(c + 12);
    }
    return 0;
}

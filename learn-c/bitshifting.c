#include <stdio.h>

int main(int argc, char *argv[]) {
    // Each left shift multiplies by 2
    unsigned char x = 1;             // 00000001
    printf("x << 0 : %d\n", x << 0); // 00000001 (1)
    printf("x << 1 : %d\n", x << 1); // 00000010 (2)
    printf("x << 2 : %d\n", x << 2); // 00000100 (4)
    printf("x << 3 : %d\n", x << 3); // 00001000 (8)
    printf("x << 7 : %d\n", x << 7); // 10000000 (8)

    return 0;
}

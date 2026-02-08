#include <stdio.h>

int main(int argc, char *argv[]) {
    // Each left shift multiplies by 2
    unsigned char x = 1;             // 00000001
    printf("x << 0 : %d\n", x << 0); // 00000001 (1)
    printf("x << 1 : %d\n", x << 1); // 00000010 (2)
    printf("x << 2 : %d\n", x << 2); // 00000100 (4)
    printf("x << 3 : %d\n", x << 3); // 00001000 (8)
    printf("x << 7 : %d\n", x << 7); // 10000000 (8)

    // Setting specific bits in a byte:
    // Position:  0  1  2  3  4  5  6  7
    // Bit:      [1][0][1][1][0][0][0][0]
    // Shift:     7  6  5  4  3  2  1  0
    unsigned char byte = 0;

    // Set bit 0:
    byte |= (1 << 7); // 00000000 | 10000000 = 10000000
    // Set bit 2:
    byte |= (1 << 5); // 10000000 | 00100000 = 10100000
    // Set bit 3:
    byte |= (1 << 4); // 10100000 | 00100000 = 10110000

    return 0;
}

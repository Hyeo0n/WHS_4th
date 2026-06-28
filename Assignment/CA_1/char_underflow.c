#include <stdio.h>
#include <limits.h>

int main() {
    char value = CHAR_MIN;  // -128
    printf("Original value: %d\n", value);
    value = value - 1;      // -128 - 1 = 127 (언더플로)
    printf("Value after subtracting 1: %d\n", value);
    return 0;
}

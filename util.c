#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void print(int64_t n) {
    printf("%ld ", n);
}

void println(int64_t n) {
    printf("%ld\n", n);
}

int64_t readnum() {
    int64_t n;
    if (scanf("%ld", &n) != 1) {
        printf("mini runtime: Couldn't read integer\n");
    }
    return n;
}

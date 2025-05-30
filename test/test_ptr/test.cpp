/*
 * test code
 * g++ -shared -fPIC -o test.so test.cpp -g -O2
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" {

uint64_t count = 0;

int func0() {
    //printf("called func0() %d\n", count);
    return count++;
}

int64_t func0_64() {
    //printf("called func0() %d\n", count);
    return count++;
}

int func1(char *p0, int p1, int p2, int p3) {
    //printf("called func1(%p, %d, %d, %d)\n", (void*)p0, p1, p2, p3);
    for (int i = 0; i < 10; i++) {
        if (p0 && p0[i]) {
            printf("    %d: %c\n", i, p0[i]);
        } else {
            break;
        }
    }
    return p1 + p2 + p3 + 12340000;
}

float floatMulAdd(float p0, double p1, int64_t p2) {
    float r = p0 * p1 + p2;
    printf("    called floatMulAdd(%f, %f, %ld) = %f\n", p0, p1, p2, r);
    return r;
}

double doubleMulAdd(float p0, int p1, double p2) {
    double r = p0 * p1 + p2;
    printf("    called doubleMulAdd(%f, %d, %f) = %f\n", p0, p1, p2, r);
    return r;
}

}

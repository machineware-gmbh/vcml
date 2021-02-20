#include <unistd.h>

int global_a = 4;
long global_b = 0x42;

int func_c(int x) {
    return x < 0 ? -2 * x : x << 5;
}

int _start() {
    ssize_t len = write(1, "hello world!", 12);
    _exit(len != 12);
}

// gcc -O3 -nostartfiles -static -o elf.elf elf.c


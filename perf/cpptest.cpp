#include "../src/block_allocator.hh"

#include <stdio.h>

using namespace std;

int main() {
    GJAlloc<int> gja;
    printf("allocing %p.\n", gja.allocate(1));
    printf("allocing %p.\n", gja.allocate(1));
    printf("allocing %p.\n", gja.allocate(1));
    printf("allocing %p.\n", gja.allocate(1));
    printf("allocing %p.\n", gja.allocate(1));
    printf("allocing %p.\n", gja.allocate(1));
    printf("allocing %p.\n", gja.allocate(1));

    return 0;
}

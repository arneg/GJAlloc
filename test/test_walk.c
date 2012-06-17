#include "gjalloc.h"
#include <stdlib.h>
#include <stdio.h>

#define N 10
struct ba_local a;

struct foo {
    size_t n;
    uint64_t padding[5];
};

struct foo ** list;

void relocate_simple(void * start, void * stop, ptrdiff_t diff) {
    struct foo * n = (struct foo*)start;
    
    while (n < (struct foo*)stop) {
	list[n->n] += diff;
	n++;
    }
}

void callback(struct foo * start, struct foo * stop, void * data) {
    while (start < stop) {
	fprintf(stderr, "is there: %llu\n", (unsigned long long)start->n);
	start++;
    }
}


int main() {
    size_t i;

    list = (struct foo**)malloc(N*sizeof(struct foo));
    ba_init_local(&a, sizeof(struct foo), 16, 512, relocate_simple, NULL);

    for (i = 0; i < N; i++) {
	list[i] = ba_lalloc(&a);
	list[i]->n = i;
    }

    ba_lfree(&a, list[5]);
    ba_lfree(&a, list[7]);
    ba_lfree(&a, list[2]);
    ba_walk_local(&a, callback, NULL);


    return 0;
}

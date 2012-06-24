#include "gjalloc.h"
#include <stdlib.h>
#include <stdio.h>

#define N 64
struct ba_local a;

struct foo {
    unsigned long long n;
    int free;
    uint64_t trigger;
    uint64_t padding[4];
};

struct foo ** list;

void relocate_simple(void * start, void * stop, ptrdiff_t diff) {
    struct foo * n = (struct foo*)start;
    
    fprintf(stderr, "relocate_simple(%p, %p, %llu)\n", start, stop, (unsigned long long)diff);
    fprintf(stderr, "[%llu ... %llu] \n", (unsigned long long)n->n, (unsigned long long)((struct foo *)stop - 1)->n);
    while (n < (struct foo*)stop) {
	ba_simple_rel_pointer(list + n->n, diff);
	n++;
    }
}

void callback(struct foo * start, struct foo * stop, void * data) {
    fprintf(stderr, "callback walking over [%llu..%llu]\n", start->n, (stop-1)->n);

    while (start < stop) {
//	fprintf(stderr, "is there: %llu\n", (unsigned long long)start->n);
	if (start->free) {
	    fprintf(stderr, "walking over free block %llu\n", (unsigned long long) start->n);
	}
	start->trigger = 1;	
	start++;
    }
}


int main() {
    size_t i;

    list = (struct foo**)malloc(N*sizeof(struct foo));
    ba_init_local(&a, sizeof(struct foo), 16, 512, relocate_simple, NULL);

    //ba_lreserve(&a, N);

    for (i = 0; i < N; i++) {
	list[i] = ba_lalloc(&a);
	list[i]->free = 0;
	list[i]->n = i;
    }

    list[5]->free = 1;
    list[7]->free = 1;
    list[2]->free = 1;
    ba_lfree(&a, list[5]);
    ba_lfree(&a, list[7]);
    ba_lfree(&a, list[2]);
    ba_walk_local(&a, callback, NULL);

    ba_ldestroy(&a);
    free(list);

    return 0;
}

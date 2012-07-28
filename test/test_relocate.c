#include "gjalloc.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define N 4096

struct foo {
    uint64_t padding[2];
    unsigned long long n;
    int free;
    int moved;
    uint64_t trigger;
};

struct foo ** list;

void relocate_simple(void * start, void * stop, ptrdiff_t diff) {
    struct foo * n = (struct foo*)start;
    
    fprintf(stderr, "relocate_simple(%p, %p, %llu)\n", start, stop, (unsigned long long)diff);
    fprintf(stderr, "[%llu ... %llu] \n", (unsigned long long)n->n, (unsigned long long)((struct foo *)stop - 1)->n);
    while (n < (struct foo*)stop) {
	ba_simple_rel_pointer((char*)(list + n->n), diff);
	n++;
    }
}

void check_list() {
    unsigned long long i;
    for (i = 0; i < N; i++) {
	if (!list[i]) continue;
	if (list[i]->free) {
	    fprintf(stderr, "%llu is free?\n", i);
	}
	if (i != list[i]->n) {
	    fprintf(stderr, "misplaced block %llu (n: %llu moved: %d)\n", i, list[i]->n, list[i]->moved);
	}
    }

}


void relocate(void * d, void * s, size_t bytes, void*data) {
    struct foo * dst = (struct foo*)d;
    struct foo * src = (struct foo*)s;
    ptrdiff_t offset;

    for (offset = 0; offset < bytes; offset += sizeof(struct foo)) {
	if (dst->n < N && list[dst->n] == dst)
	    fprintf(stderr, "relocating into non-free block %p\n", dst);
	memcpy(dst, src, sizeof(struct foo));
	if (src->free) {
	    fprintf(stderr, "moving free block %p\n", dst);
	}
	if (list[src->n] != src) {
	    fprintf(stderr, "moving misplaced block\n");
	}
	if (list[src->n]->moved) {
	    fprintf(stderr, "moving block twice\n");
	}
	list[src->n] = dst;
	dst->moved = 1;
	dst++;
	src++;
    }
}

TEST_INIT(foo);

static inline void free_foo(unsigned long long i) {
    if (list[i]->n != i) fprintf(stderr, "found %llu in wrong slot %llu\n", list[i]->n, i);
    if (list[i]->free) fprintf(stderr, "double freeing %p\n", list[i]);
    list[i]->free = 1;
    TEST_FREE(foo, list[i]);
    list[i] = NULL;
}

int main() {
    unsigned long long i;

    list = (struct foo**)malloc(N*sizeof(struct foo));
    DYNAMIC_INIT(foo);

    for (i = 0; i < N; i++) {
	unsigned long long j;
	list[i] = TEST_ALLOC(foo);
	for (j = 0; j < i; j++)
	    if (list[j] == list[i]) {
		fprintf(stderr, "allocated the same chunk twice: %p\n", list[i]);
	    }
	
	list[i]->free = 0;
	list[i]->n = i;
	list[i]->moved = 0;
    }

    for (i = 0; i + 10 < N; i+=10) {
	int j;
	/* free 8 out of 10 */
	for (j = 0; j < 8; j++) {
	    free_foo(i+j);
	}
    }

    check_list();

#ifdef TEST_DEFRAGMENT
    fprintf(stderr, "density before defragment: %f\n", TEST_DENSITY(foo));
    TEST_DEFRAGMENT(foo, relocate, NULL);
    fprintf(stderr, "density after defragment:  %f\n", TEST_DENSITY(foo));
#endif

    check_list();

    for (i = 0; i < N; i++) {
	if (list[i]) free_foo(i);
    }

    TEST_DEINIT(foo);
    free(list);

    return 0;
}

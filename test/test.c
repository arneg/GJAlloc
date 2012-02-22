#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>

#define M 14000000
#define N 14000000
#define SIZE	32

struct foo {
    char p[SIZE];
};

static struct foo blue = { "foobarflubar" };

unsigned long long int diff(struct timespec t1, struct timespec t2) {
    return (t2.tv_sec - t1.tv_sec) * 1000000000ULL + (t2.tv_nsec - t1.tv_nsec);
}

void shuffle(uint32_t * array, size_t n)
{
    if (n > 1) {
        size_t i;
	for (i = 0; i < n - 1; i++) {
	  size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
	  uint32_t t = array[j];
	  array[j] = array[i];
	  array[i] = t;
	}
    }
}

void **p;
size_t mdiff = 0, fdiff = 0;
uint32_t num_pages = 0;
size_t bytes = 0, bytes_base;
TEST_INIT(foo);

static inline size_t used_bytes(const struct mallinfo info) {
    return (size_t)info.uordblks + (size_t)info.hblkhd;
}

long int run2(long int n, uint32_t * shuffler) {
    long int i, j;
    struct timespec t1, t2;
    long int runs = N/n;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < runs; i++) {
	for (j = 0; j < n; j++) {
	    p[j] = TEST_ALLOC(foo);
	}
	for (j = 0; j < n; j++) {
	    long int k = shuffler[j];
	    TEST_FREE(foo, p[k]);
	}
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    mdiff += diff(t1, t2);

    // get memory use
    for (j = 0; j < n; j++) {
	p[j] = TEST_ALLOC(foo);
    }
    TEST_NUM_PAGES(foo, num_pages);
    bytes = used_bytes(mallinfo()) - bytes_base;
    for (j = 0; j < n; j++) {
	long int k = shuffler[j];
	TEST_FREE(foo, p[k]);
    }

    return runs * n;
}

int main(int argc, char ** argv) {
    long int i;
    p = (void**)malloc(sizeof(void*) * N);
    uint32_t * map = (uint32_t*)malloc(sizeof(uint32_t) * N);
    srand ( time(NULL) );
    long int loops;

#ifndef REAL_RANDOM
# ifdef SEED
    srand(SEED);
# else
    srand(0xaeaeaeaf);
# endif
#endif

    bytes_base = used_bytes(mallinfo());

#ifdef DYNAMIC_INIT
    DYNAMIC_INIT(foo);
#endif

    for (i = 0; i < N; i++)
	map[i] = i;
    for (i = 10; i < M; i *= 2) {
	long int n;
	mdiff = fdiff = 0;
#ifdef RANDOM
	shuffle(map, i);
#endif
	n = run2(i, map);
	printf("%ld\t%.1f\t%lu\t%u\n", i, (double)mdiff/(n), bytes, num_pages);
    }

    TEST_DEINIT(foo);

    return 0;
}

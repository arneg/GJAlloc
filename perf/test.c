#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#ifndef NO_MEM_USAGE
# include <malloc.h>
#endif

#ifdef BA_DEBUG
/* more usable with memcheck and ba_check_allocator and probably big enoug
 * to catch bugs */
#define M 40000
#define N 40000
#else
#define M 14000000
#define N 14000000
#endif
#define SIZE	32

struct foo {
    void ** ptr;
    char p[SIZE-sizeof(void*)];
};

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

void relocate_simple(void * ptr, void * stop, ptrdiff_t diff) {
    struct foo * f = (struct foo *)ptr;

    fprintf(stderr, "relocating from %p to %p\n", ptr, stop);
    while (f < (struct foobar *)stop) {
	ba_simple_rel_pointer(f->ptr, diff);
    }
}

#ifndef NO_MEM_USAGE
static inline size_t used_bytes(const struct mallinfo info) {
    return (size_t)info.uordblks + (size_t)info.hblkhd;
}
#endif

long int run2(long int n, uint32_t * shuffler) {
    long int i, j;
    struct timespec t1, t2;
    long int runs = N/n;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < runs; i++) {
	for (j = 0; j < n; j++) {
	    p[j] = TEST_ALLOC(foo);
	    ((struct foo *)p[j])->ptr = p + j;
	}
#ifdef TEST_FREE
	for (j = 0; j < n; j++) {
#ifdef RANDOM
	    long int k = shuffler[j];
	    TEST_FREE(foo, p[k]);
#else
	    TEST_FREE(foo, p[j]);
#endif
	}
#elif defined(TEST_FREEALL)
	TEST_FREEALL(foo);
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    mdiff += diff(t1, t2);
#ifndef NO_MEM_USAGE
    // get memory use
    for (j = 0; j < n; j++) {
	p[j] = TEST_ALLOC(foo);
    }
    TEST_NUM_PAGES(foo, num_pages);
    bytes = used_bytes(mallinfo()) - bytes_base;
#ifdef TEST_FREE
    for (j = 0; j < n; j++) {
# ifdef RANDOM
	long int k = shuffler[j];
	TEST_FREE(foo, p[k]);
# else
	TEST_FREE(foo, p[j]);
# endif
    }
#elif defined(TEST_FREEALL)
    TEST_FREEALL(foo);
#endif
#endif

    return runs * n;
}

int main(int argc, char ** argv) {
    long int i;
    p = (void**)malloc(sizeof(void*) * N);
    uint32_t * map = (uint32_t*)malloc(sizeof(uint32_t) * N);
    srand ( time(NULL) );

#ifndef REAL_RANDOM
# ifdef SEED
    srand(SEED);
# else
    srand(0xaeaeaeaf);
# endif
#endif

#ifndef NO_MEM_USAGE
    bytes_base = used_bytes(mallinfo());
#endif

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
	printf("%ld\t%.1f\t%lu\t%u\n", i, (double)mdiff/(n), (long unsigned)bytes, num_pages);
    }

    TEST_DEINIT(foo);

    return 0;
}

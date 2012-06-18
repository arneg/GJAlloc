#include "gjalloc.h"
struct foobar {
    void ** ptr;
    char p[32-sizeof(void*)];
};
void simple_reloc(void * ptr, void * stop, ptrdiff_t diff) {
    struct foobar * f = (struct foobar *)ptr;

    fprintf(stderr, "relocating from %p to %p\n", ptr, stop);
    while (f < (struct foobar *)stop) {
	*(f->ptr) += diff;
    }
}
#define TEST_INIT(t)	static struct ba_local allocator;
#define DYNAMIC_INIT(t)	do { ba_init_local(&allocator, sizeof(struct foo), 32,\
					   512, simple_reloc, NULL); } while(0)
#define TEST_ALLOC(t)	ba_lalloc(&allocator);
#define TEST_FREE(t, p)	ba_lfree(&allocator, p);
#define TEST_DEINIT(t)	ba_ldestroy(&allocator);
#define TEST_NUM_PAGES(t, v)	do { v = allocator.a ? allocator.a->num_pages : 1; } while(0)

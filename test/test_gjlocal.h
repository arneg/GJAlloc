#include "gjalloc.h"
void simple_reloc(void * ptr, size_t n, ptrdiff_t diff) { 
    fprintf(stderr, "relocating %lu blocks\n", (unsigned long)n);
}
#define TEST_INIT(t)	static struct ba_local allocator;
#define DYNAMIC_INIT(t)	do { ba_init_local(&allocator, sizeof(struct foo), 32,\
					   512, simple_reloc, NULL); } while(0)
#define TEST_ALLOC(t)	ba_lalloc(&allocator);
#define TEST_FREE(t, p)	ba_lfree(&allocator, p);
#define TEST_DEINIT(t)	ba_ldestroy(&allocator);
#define TEST_NUM_PAGES(t, v)	do { v = allocator.a ? allocator.a->num_pages : 1; } while(0)

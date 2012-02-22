#include "challoc.h"
#define TEST_INIT(t)	static ChunkAllocator *allocator;
#define DYNAMIC_INIT(t)	allocator = chcreate(512, sizeof(struct t))
#define TEST_ALLOC(t)	challoc(allocator)
#define TEST_FREE(t, p)	chfree(allocator, p)
#define TEST_DEINIT(t)	chdestroy(&allocator)
#define TEST_NUM_PAGES(t, v)	do { v = 0; } while(0)

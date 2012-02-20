#include "block_allocator.h"
#define TEST_INIT(t)	static block_allocator allocator = BA_INIT(sizeof(struct t), 0)
#define TEST_ALLOC(t)	ba_alloc(&allocator);
#define TEST_FREE(t, p)	ba_free(&allocator, p);
#define TEST_DEINIT(t)	ba_destroy(&allocator);
#define TEST_NUM_PAGES(t, v)	do { v = allocator.num_pages; } while(0)

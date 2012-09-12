#include "gjalloc.h"
#define TEST_INIT(t)	static struct ba_temporary allocator;
#define DYNAMIC_INIT(t) ba_init_temporary(&allocator, sizeof(struct t), 512);
#define TEST_ALLOC(t)	ba_talloc(&allocator);
#define TEST_FREE(t, p)	
#define TEST_FREEALL(t)	ba_tdestroy(&allocator);
#define TEST_DEINIT(t)	do { } while (0)
#define TEST_NUM_PAGES(t, v)	do { v = allocator.num_pages; } while(0)
#define TEST_WALK(t, callback, data)	ba_walk_temporary(&allocator, callback, data)

#include "gjalloc.h"
#define TEST_INIT(t)	static struct block_allocator allocator = BA_INIT(sizeof(struct t), 512)
#define DYNAMIC_INIT(t)
#define TEST_ALLOC(t)	ba_alloc(&allocator);
#define TEST_FREE(t, p)	
#define TEST_FREEALL(t) ba_free_all(&allocator);
#ifdef BA_STATS
# define TEST_DEINIT(t)	do { ba_print_stats(&allocator); ba_destroy(&allocator); } while (0)
#else
# define TEST_DEINIT(t)	do { ba_destroy(&allocator); } while (0)
#endif
#define TEST_NUM_PAGES(t, v)	do { v = allocator.num_pages; } while(0)
#define TEST_WALK(t, callback, data)	ba_walk(&allocator, callback, data)

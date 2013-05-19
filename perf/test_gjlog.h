#include "gjalloc.h"
#define TEST_INIT(t)	static struct ba_log allocator;
#define DYNAMIC_INIT(t)	ba_log_init(&allocator, sizeof(struct t), 512)
#define TEST_ALLOC(t)	ba_log_alloc(&allocator)
#define TEST_FREE(t, p)	ba_log_free(&allocator, p)
#define TEST_DEINIT(t)	do { } while(0)
#define TEST_NUM_PAGES(t, v)	do { v = allocator.size; } while(0)

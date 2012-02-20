#include "block_allocator.hh"
#define TEST_INIT(t)	GJAlloc<t> a
#define TEST_ALLOC(t)	a.allocate(1)
#define TEST_FREE(t, p)	a.deallocate((t*)p, 1)
#define TEST_DEINIT(t)	
#define TEST_NUM_PAGES(t, v)	do { v = a.num_pages(); } while (0)

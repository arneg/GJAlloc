#include "FSBAllocator.hh"
#define TEST_CLASS(t)	FSBAllocator<t>
#define TEST_INIT(t)	TEST_CLASS(t) a
#define TEST_ALLOC(t)	a.allocate(1)
#define TEST_FREE(t, p)	a.deallocate((t*)p, 1)
#define TEST_DEINIT(t)	
#define TEST_NUM_PAGES(t, v)	do { v = 0; } while(0)

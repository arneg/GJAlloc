#include <bits/allocator.h>
#define TEST_CLASS(t)	std::allocator<t>
#define TEST_INIT(t)	TEST_CLASS(t) * a
#define DYNAMIC_INIT(t)	a = new TEST_CLASS(t)
#define TEST_ALLOC(t)	a->allocate(1)
#define TEST_FREE(t, p)	a->deallocate((t*)p, 1)
#define TEST_DEINIT(t)	delete a
#define TEST_NUM_PAGES(t, v)	do { v = 0; } while(0)

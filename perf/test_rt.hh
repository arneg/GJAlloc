#include "rtAllocator.h"
#define TEST_CLASS(t)	rtAllocator
#define TEST_INIT(t)	TEST_CLASS(t) * a
#define DYNAMIC_INIT(t)	a = new TEST_CLASS(t)
#define TEST_ALLOC(t)	a->alloc(sizeof(t))
#define TEST_FREE(t, p)	a->free((void*)p)
#define TEST_DEINIT(t)	delete a
#define TEST_NUM_PAGES(t, v)	do { v = 0; } while(0)

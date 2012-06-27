#include <stdlib.h>
#define TEST_INIT(t)	
#define TEST_ALLOC(t)	malloc(sizeof(struct t))
#define TEST_FREE(t, p)	free(p)
#define TEST_DEINIT(t)	
#define TEST_NUM_PAGES(t, v)	do { v = 0; } while(0)
#define DYNAMIC_INIT(t)	
#define TEST_WALK(t, callback, data) 

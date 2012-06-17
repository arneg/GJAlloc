#include <list>
#include "block_allocator.hh"
#include <stdlib.h>
#include <malloc.h>
#define SIZE	16

struct foo {
    char p[16];
};

unsigned long long int diff(struct timespec t1, struct timespec t2) {
    return (t2.tv_sec - t1.tv_sec) * 1000000000ULL + (t2.tv_nsec - t1.tv_nsec);
}

int main() {
    TEST_CLASS(foo) allocref;
    typedef std::list<foo, TEST_CLASS(foo) > L;
    struct mallinfo m;
    struct timespec t1, t2;
    L l;
    struct foo t = {"foobar"};

    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int i = 0; i < 10000000; i++) {
	l.push_back(t);
    }
    for (L::iterator it = l.begin(); it != l.end(); std::advance(it, 2)) {
	it = l.erase(it);
    }
    for (int i = 0; i < 10000000; i++) {
	l.push_back(t);
    }
    for (L::iterator it = l.begin(); it != l.end(); std::advance(it, 6)) {
	it = l.erase(it);
    }
    for (int i = 0; i < 10000000; i++) {
	l.push_back(t);
    }
    m = mallinfo();
    l.clear();

    clock_gettime(CLOCK_MONOTONIC, &t2);

    printf("%lf s\t%d bytes\n", diff(t1, t2)*1E-9, m.uordblks);

    return 0;
}

#include <list>
#include "block_allocator.hh"
#define SIZE	16

struct foo {
    char p[16];
};

int main() {
    TEST_CLASS(foo) allocref;
    typedef std::list<foo, TEST_CLASS(foo) > L;
    L l;
    struct foo t = {"foobar"};
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
    l.clear();

    return 0;
}

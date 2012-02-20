#ifndef BLOCK_ALLOCATOR_HH
#include <new>
#include <vector>
#include <cassert>
#include <iostream> // DEBUG
//#define PHONY_ALLOC_SCHEME
#include "block_allocator.h"
#define BLOCK_ALLOCATOR_HH


template <size_t BA_N, size_t BLOCKS> class GJAlloc_Singleton
{
    struct MV {
	block_allocator allocator;

	MV() {
	    ba_init(&allocator, BA_N, BLOCKS);
	}
    };
    static MV mv;

public:
    static void *allocate() {
	return ba_alloc(&mv.allocator);
    }

    static void deallocate(void *p) {
	ba_free(&mv.allocator, p);
    }

    static uint32_t num_pages() {
	return mv.allocator.num_pages;
    }

    static size_t count_blocks() {
	size_t num, size;
	ba_count_all(&(mv.allocator), &num, &size);
	return num;
    }

    static size_t count_bytes() {
	size_t num, size;
	ba_count_all(&(mv.allocator), &num, &size);
	return size;
    }
};

template <size_t BA_N, size_t BLOCKS> typename GJAlloc_Singleton<BA_N,BLOCKS>::MV GJAlloc_Singleton<BA_N,BLOCKS>::mv;

template <typename T, size_t BLOCKS=0> class GJAlloc {
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;


public:
    pointer allocate(size_type n, const void* = 0) {
	assert(n == 1);
	//return static_cast<pointer>(GJAlloc_Singleton<sizeof(T)>::singleton.allocate());
	//return static_cast<pointer>(singleton.allocate());
	return static_cast<pointer>(GJAlloc_Singleton<sizeof(T),BLOCKS>::allocate());
    }

    void deallocate(pointer p, size_type n) {
	assert(n == 1);

	GJAlloc_Singleton<sizeof(T),BLOCKS>::deallocate(static_cast<void*>(p));
    }

    void construct(pointer p, const T &val) {
	std::cerr << "WARNING: construct has been called." << std::endl;
	new ((void*)p) T(val);
    }

    void destroy(pointer p) {
	((T*)p)->~T();
    }

    template <typename U> struct rebind {
	typedef GJAlloc<U> other;
    };

    GJAlloc() throw() { }
    GJAlloc(const GJAlloc &a) throw() {
	std::cerr << "GJAlloc(const GJAlloc&)" << std::endl;
    }
    template <typename U> GJAlloc(const GJAlloc<U> &a) throw() {
	std::cerr << "GJAlloc(T -> U)" << std::endl;
    }
    ~GJAlloc() throw() { }

    size_type max_size() {
	return 1;
    }

    uint32_t num_pages() {
	return GJAlloc_Singleton<sizeof(T),BLOCKS>::num_pages();
    }

    size_t count_blocks() {
	return GJAlloc_Singleton<sizeof(T),BLOCKS>::count_blocks();
    }

    size_t count_bytes() {
	return GJAlloc_Singleton<sizeof(T),BLOCKS>::count_bytes();
    }
};

template <class T1, class T2>
bool operator==(const GJAlloc<T1>&, const GJAlloc<T2>&) throw() {
    return sizeof(T1)==sizeof(T2);
}

template <class T1, class T2>
bool operator!=(const GJAlloc<T1>&, const GJAlloc<T2>&) throw() {
    return sizeof(T1)!=sizeof(T2);
}
#endif

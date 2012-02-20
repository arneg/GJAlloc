#ifndef SMARTPOINTER_H
#define SMARTPOINTER_H
#include "block_allocator.hh"

template <class T, size_t BLOCKS=0> class SmartPointer {
    static GJAlloc<T, BLOCKS> allocator;
public:
    SmartPointer() {
	refs = NULL;
	t = NULL;
    }

    SmartPointer(const T &tt) {
	init(tt);
    }

    SmartPointer(const T &tt, int i) {
	init(tt);
    }

    SmartPointer(T *tt) {
	refs = new int(1);
	t = tt;
    }

    SmartPointer(const SmartPointer<T> &pt) {
	refs = NULL;

	*this = pt;
    }
    
    template<class X> SmartPointer(const SmartPointer<X, BLOCKS> &pt) {
	refs = NULL;
	*this = pt;
    }

    ~SmartPointer() {
	release();
    }

    T &operator*() {
	return *t;
    }

    const T &operator*() const {
	return *t;
    }

    T *operator->() {
	return t;
    }

    const T *operator->() const {
	return t;
    }

    int num() {
	if (refs != NULL) {
	    return *refs;
	} else {
	    return -255;
	}
    }

    SmartPointer &operator=(const SmartPointer<T,BLOCKS> &other) {
	release();
	refs = other.refs;
	if (refs != NULL) (*refs)++;
	t = other.t;

	return *this;
    }

    operator void*() {
	return static_cast<void*>(t);
    }

    // assumes that X derives T
    template<class X> SmartPointer &operator=(const SmartPointer<X,BLOCKS> &other) {
	release();
	refs = other.refs;
	if (refs != NULL) (*refs)++;
	t = other.t;

	return *this;
    }

    // These are public so that if Blabla inherits Bla,
    //
    // Blabla b;
    // SmartPointer<Blabla> p1(b);
    // SmartPointer<Bla> p2(p1);
    //
    //works.
    T *t;
    int *refs;
private:
    void release() {
	if (refs != NULL && --(*refs) == 0) {
	    allocator.deallocate(static_cast<T*>(t), 1);
	    delete refs;
	    
	    // don't need the following instructions because release either
	    // gets called by ~SmartPointer(), or by operator=, which
	    // overwrites refs and t anyway.
	    
	    // refs = NULL;
	    // t = NULL;
	}
    }

    void init(const T &tt) {
	T *heapplace = allocator.allocate(1);
	allocator.construct(heapplace, tt);

	refs = new int(1);
	t = heapplace;
    }
};

template <typename T, size_t BLOCKS> GJAlloc<T, BLOCKS> SmartPointer<T, BLOCKS>::allocator;
#endif

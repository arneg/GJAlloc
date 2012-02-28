# GJAlloc

  GJAlloc is a fixed size block allocator implementation for C. It can
  improve performance of applications that allocate many (small) blocks of
  equal size. The allocator has no overhead per individual block. The minimum
  block size is `sizeof(void*)` and allocators initialized with a block size
  smaller than that will use blocks of size `sizeof(void*)` implicitly.

  GJAlloc comes in two different internal algorithms. One uses aligned pages
  to minimize the cost of deallocations. It can be activated by defining
  `BA_USE_MEMALIGN`. It is optimized for speed and can crash when passing a
  bad pointer to `ba_free()`. It also does not make any effort to detect other
  misuse, like double frees.
  The second algorithm uses unaligned pages and a hash table to locate pages.
  It offers some fault detection. Performance may vary between the two,
  depending on architecture and application. Good luck!

  When debugging it is advised to compile with `BA_DEBUG` which will detect
  double frees and many cases of bad memory access in blocks that have been
  freed.

## Usage in C

    #include "block_allocator.h"

    block_allocator allocator = BA_INIT(block_size, blocks);

  A block allocator struct can be initialized using the `BA_INIT` macro.
  Alternatively, `ba_init` can be used.

    block_allocator allocator;
    void ba_init(block_allocator * allocator, uint32_t block_size, uint32_t blocks)

  The parameter `block_size` is the size of the individual blocks returned
  from the allocator on calls to `ba_alloc`. `blocks` can be used to set the
  number of blocks that are allocated in one page. Note that `blocks` is
  in both cases merely a hint. `ba_init` tries to adjust the pages as to fit
  into integer multiples of the memory page size. 

    void * ba_alloc(block_allocator * allocator)
    void ba_free(block_allocator * allocator, void * ptr)

    void ba_free_all(block_allocator * allocator)

  Frees all blocks (and pages) allocated in `allocator`. `ba_alloc` can still
  be used to allocate new blocks.

    void ba_destroy(block_allocator * allocator)

  Same as ba_free_all() but also frees all internally used memory.

## Usage in C++

  GJAlloc comes with `std::allocator` compatible template definitions. It can
  allocate one chunk at a time only (for use in `std::list`, `std::set`
  and `std::map`).

    #include "block_allocator.hh"

  Then you are doomed. So many options.
  The simple usage:

    std::list<foo, GJAlloc<foo> > l; // you're good, have fun
    std::list<foo, GJAlloc<foo, 1024> > l; // 1024 blocks per allocator page now

  More fun can be had by using the GJAlloc directly.
  Note: In this cases, make sure you keep the GJAlloc object around until you
        have no further use for any of the allocated blocks, they will be
	freed when the last GJAlloc for matching type size (and blocks per
	page setting) goes out of scope.

    GJAlloc<double> double_alloc;
    double *d(double_alloc.allocate(1));
    double_alloc.deallocate(d);

    GJAlloc<double, 2048> double_alloc; // 2048 blocks per allocator page
    double *d(double_alloc.allocate(1));
    double_alloc.deallocate(d);

  Ultimate pleasures:

    #include "smartpointer.hh"

    GJAlloc<double, 0, SmartPointer<double> > double_alloc;
    SmartPointer<double> d(double_alloc.allocate(1));

  Notice: no deallocate here. Each block will be automatically freed when
  the last SmartPointer pointing to it goes out of scope.

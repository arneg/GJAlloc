#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <stdint.h>
#ifdef BA_MEMTRACE
 #include <stdio.h>
#endif

#define CONCAT(X, Y)	X##Y
#define XCONCAT(X, Y)	CONCAT(X, Y)

#define DOUBLE_LINK(head, o)	do {		\
    (o)->prev = NULL;				\
    (o)->next = head;				\
    if (head) (head)->prev = (o);		\
    (head) = (o);				\
} while(0)

#define DOUBLE_UNLINK(head, o)	do {		\
    if ((o)->prev) (o)->prev->next = (o)->next;	\
    else (head) = (o)->next;			\
    if ((o)->next) (o)->next->prev = (o)->prev;	\
    (o)->next = (o)->prev = NULL;		\
} while(0)

#define DOUBLE_SHIFT(head)	do {		\
    (head)->prev = NULL;			\
    (head) = (head)->next;			\
    if (head) (head)->prev = NULL;		\
} while(0)

#define SINGLE_SHIFT(head)	do {		\
    (head) = (head)->next;			\
} while(0)

#define SINGLE_LINK(head, o)	do {		\
    (o)->next = head;				\
    (head) = (o);				\
} while(0)

//#define BA_DEBUG
//#define BA_USE_MEMALIGN

#ifdef HAS___BUILTIN_EXPECT
#define likely(x)	(__builtin_expect((x), 1))
#define unlikely(x)	(__builtin_expect((x), 0))
#else
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif
#define BA_MARK_FREE	0xF4337575
#define BA_MARK_ALLOC	0x4110C375
#define BA_ALLOC_INITIAL	8
//#define BA_HASH_THLD		8
#define BA_MAX_EMPTY		8

#ifdef BA_DEBUG
#define IF_DEBUG(x)	x
#else
#define IF_DEBUG(x)
#endif

#ifndef MEM_RW_RANGE
# define MEM_RW_RANGE(x, y)
#endif
#ifndef MEM_RW
# define MEM_RW(x)
#endif
#ifndef MEM_WO
# define MEM_WO(x)
#endif

#ifndef EXPORT
# define EXPORT
#endif

#ifndef INLINE
# define INLINE __inline__
#endif
#ifndef ATTRIBUTE
# define ATTRIBUTE(x)	__attribute__(x)
#endif

#ifndef ba_error
#include <stdio.h>
#include <unistd.h>

static inline void __error(int line, char * file, char * msg) {
    fprintf(stderr, "%s:%d\t%s\n", file, line, msg);
    _exit(1);
}
#define ba_error(x...)	do { snprintf(errbuf, 127, x); __error(__LINE__, __FILE__, errbuf); } while(0)

#define BA_NEED_ERRBUF
extern char errbuf[];
#endif

#ifdef BA_DEBUG
# ifdef BA_USE_MEMALIGN
#  define BA_ERROR(x...)	do { fprintf(stderr, "ERROR at %s:%d\n", __FILE__, __LINE__); ba_show_pages(a); ba_error(x); } while(0)
# else
#  define BA_ERROR(x...)	do { fprintf(stderr, "ERROR at %s:%d\n", __FILE__, __LINE__); ba_print_htable(a); ba_show_pages(a); ba_error(x); } while(0)
# endif
#else
#define BA_ERROR(x...)	do { ba_error(x); } while(0)
#endif

typedef struct ba_block_header * ba_block_header;
typedef struct ba_page * ba_page;

#ifdef BA_STATS
struct block_alloc_stats {
    size_t st_max;
    size_t st_used;
    size_t st_max_pages;
    size_t st_max_allocated;
    char * st_name;
    size_t free_fast1, free_fast2, free_full, free_empty, max, full, empty;
    size_t find_linear, find_hash;
    struct mallinfo st_mallinfo;
};
# define BA_INIT_STATS(block_size, blocks, name) {\
    0/*st_max*/,\
    0/*st_used*/,\
    0/*st_max_pages*/,\
    0/*st_max_allocated*/,\
    name/*st_name*/,\
    0,0,0,0,0,0,0,\
    0,0}
#else
# define BA_INIT_STATS(block_size, blocks, name)
#endif

struct block_allocator {
#ifdef BA_USE_MEMALIGN
    uint32_t block_size;
    uint32_t magnitude;
#else
    size_t offset;
#endif
    ba_block_header free_blk; /* next free blk in alloc */
    ba_page alloc; /* current page used for allocating */
#ifndef BA_USE_MEMALIGN
    ba_page last_free;
#endif
#ifdef BA_USE_MEMALIGN
    size_t offset;
#else
    uint32_t block_size;
    uint32_t magnitude;
#endif
    ba_page first; /* doube linked list of other pages (!free,!full,!alloc) */
#ifndef BA_USE_MEMALIGN
    ba_page * pages;
#else
    ba_page full; /* doube linked list of full pages */
#endif
    ba_page empty; /* single linked list of empty pages */
    uint32_t empty_pages, max_empty_pages;
    uint32_t blocks;
#ifndef BA_USE_MEMALIGN
    uint32_t allocated;
#endif
    uint32_t num_pages;
    char * blueprint;
#ifdef BA_STATS
    struct block_alloc_stats stats;
#endif
};
typedef struct block_allocator block_allocator;
#ifdef BA_USE_MEMALIGN
#define BA_INIT(block_size, blocks, name) {\
    block_size/*block_size*/,\
    0/*magnitude*/,\
    NULL/*free_blk*/,\
    NULL/*alloc*/,\
    0/*offset*/,\
    NULL/*first*/,\
    NULL/*full*/,\
    NULL/*empty*/,\
    0/*empty_pages*/,\
    BA_MAX_EMPTY/*max_empty_pages*/,\
    blocks/*blocks*/,\
    0/*num_pages*/,\
    NULL/*blueprint*/,\
    BA_INIT_STATS(block_size, blocks, name)\
}
#else
#define BA_INIT(block_size, blocks, name) {\
    0/*offset*/,\
    NULL/*free_blk*/,\
    NULL/*alloc*/,\
    NULL/*last_free*/,\
    block_size/*block_size*/,\
    0/*magnitude*/,\
    NULL/*first*/,\
    NULL/*pages*/,\
    NULL/*empty*/,\
    0/*empty_pages*/,\
    BA_MAX_EMPTY/*max_empty_pages*/,\
    blocks/*blocks*/,\
    0/*allocated*/,\
    0/*num_pages*/,\
    NULL/*blueprint*/,\
    BA_INIT_STATS(block_size, blocks, name)\
}
#endif

struct ba_page {
    struct ba_block_header * first;
    ba_page next, prev;
#ifndef BA_USE_MEMALIGN
    ba_page hchain;
#endif
    uint32_t used;
};

struct ba_block_header {
    struct ba_block_header * next;
#ifdef BA_DEBUG
    uint32_t magic;
#endif
};

#define BA_ONE	((ba_block_header)1)

EXPORT void ba_low_free(struct block_allocator * a,
				    ba_page p, ba_block_header ptr);
EXPORT void ba_show_pages(const struct block_allocator * a);
EXPORT void ba_init(struct block_allocator * a, uint32_t block_size,
			 uint32_t blocks);
EXPORT void ba_low_alloc(struct block_allocator * a);
#ifndef BA_USE_MEMALIGN
EXPORT void ba_find_page(struct block_allocator * a,
			      const void * ptr);
#endif
EXPORT void ba_remove_page(struct block_allocator * a, ba_page p);
#ifdef BA_DEBUG
EXPORT void ba_print_htable(const struct block_allocator * a);
EXPORT void ba_check_allocator(struct block_allocator * a, char*, char*, int);
#endif
#ifdef BA_STATS
EXPORT void ba_print_stats(struct block_allocator * a);
#endif
EXPORT void ba_free_all(struct block_allocator * a);
EXPORT void ba_count_all(struct block_allocator * a, size_t *num, size_t *size);
EXPORT void ba_destroy(struct block_allocator * a);

#define BA_PAGE(a, n)   ((a)->pages[(n) - 1])
#define BA_BLOCKN(a, p, n) ((ba_block_header)(((char*)(p+1)) + (n)*((a)->block_size)))
#define BA_CHECK_PTR(a, p, ptr)	((size_t)((char*)ptr - (char*)(p)) <= (a)->offset)
#define BA_LASTBLOCK(a, p) ((ba_block_header)((char*)p + (a)->offset))

#ifdef BA_STATS
# define INC(X) do { (a->stats.X++); } while (0)
# define IF_STATS(x)	do { x; } while(0)
#else
# define INC(X) do { } while (0)
# define IF_STATS(x)	do { } while(0)
#endif

#ifdef BA_MEMTRACE
static int _do_mtrace;
static char _ba_buf[256];
static inline void emit(int n) {
    if (!_do_mtrace) return;
    fflush(NULL);
    write(3, _ba_buf, n);
    fflush(NULL);
}

static ATTRIBUTE((constructor)) void _________() {
    _do_mtrace = !!getenv("MALLOC_TRACE");
}

# define PRINT(fmt, args...)     emit(snprintf(_ba_buf, sizeof(_ba_buf), fmt, args))
#endif

ATTRIBUTE((always_inline,malloc))
static INLINE void * ba_alloc(struct block_allocator * a) {
    ba_block_header ptr;
#ifdef BA_STATS
    struct block_alloc_stats *s = &a->stats;
#endif

    if (!a->free_blk) {
	ba_low_alloc(a);
#ifdef BA_DEBUG
	ba_check_allocator(a, "after ba_low_alloc", __FILE__, __LINE__);
#endif
    }
    ptr = a->free_blk;
#ifndef BA_CHAIN_PAGE
    if (ptr->next == BA_ONE) {
	a->free_blk = (ba_block_header)(((char*)ptr) + a->block_size);
	a->free_blk->next = (ba_block_header)(size_t)!(a->free_blk == BA_LASTBLOCK(a, a->alloc));
    } else
#endif
	a->free_blk = ptr->next;
#ifdef BA_DEBUG
    ((ba_block_header)ptr)->magic = BA_MARK_ALLOC;
#endif
#ifdef BA_MEMTRACE
    PRINT("%% %p 0x%x\n", ptr, a->block_size);
#endif
#ifdef BA_STATS
    if (++s->st_used > s->st_max) {
      s->st_max = s->st_used;
#ifndef BA_USE_MEMALIGN
      s->st_max_allocated = a->allocated;
#endif
      s->st_max_pages = a->num_pages;
      s->st_mallinfo = mallinfo();
    }
#endif
#ifdef BA_DEBUG
	ba_check_allocator(a, "after ba_alloc", __FILE__, __LINE__);
#endif
    
    return ptr;
}


ATTRIBUTE((always_inline))
static INLINE void ba_free(struct block_allocator * a, void * ptr) {
    ba_page p;

#ifdef BA_MEMTRACE
    PRINT("%% %p\n", ptr);
#endif

#ifdef BA_DEBUG
    if (a->empty_pages == a->num_pages) {
	BA_ERROR("we did it!\n");
    }
    if (((ba_block_header)ptr)->magic == BA_MARK_FREE) {
	BA_ERROR("double freed somethign\n");
    }
    //memset(ptr, 0x75, a->block_size);
    ((ba_block_header)ptr)->magic = BA_MARK_FREE;
#endif

#ifdef BA_STATS
    a->stats.st_used--;
#endif
#ifdef BA_USE_MEMALIGN
    p = (ba_page)((uintptr_t)ptr &
			  ((~(uintptr_t)0) << (a->magnitude)));
    if (likely(p == a->alloc)) {
#else
    if (likely(BA_CHECK_PTR(a, a->alloc, ptr))) {
#endif
	INC(free_fast1);
	((ba_block_header)ptr)->next = a->free_blk;
	a->free_blk = (ba_block_header)ptr;
	return;
    }

#ifdef BA_USE_MEMALIGN
    INC(free_fast2);
#else

    if (BA_CHECK_PTR(a, a->last_free, ptr)) {
	p = a->last_free;
	INC(free_fast2);
    } else {
	ba_find_page(a, ptr);
	p = a->last_free;
    }
#endif
    ((ba_block_header)ptr)->next = p->first;
    p->first = (ba_block_header)ptr;
    if ((--p->used) && (((ba_block_header)ptr)->next)) {
	INC(free_fast2);
	return;
    }

    ba_low_free(a, p, (ba_block_header)ptr);
}

/* we assume here that malloc has sizeof(void*) bytes of overhead */
#define BLOCK_HEADER_SIZE (sizeof(struct ba_page) + sizeof(void*))

#define MS(x)	#x

#ifdef BA_USE_MEMALIGN
#define LOW_PAGE_LOOP2(a, label, C...)	do {			\
    char __c = 4;						\
    const ba_page T[4] = { a->full, a->first, a->alloc, a->empty };	\
    ba_page p;							\
    retry ## label:						\
    p = T[__c-1];						\
    while (p) {							\
	ba_page t = p->next;					\
	do { C; goto SURVIVE ## label; } while(0);		\
	goto label; SURVIVE ## label: p = t;			\
    }								\
    if (--__c) goto retry ## label;				\
label:								\
    0;								\
} while (0)
#else
/* goto considered harmful */
#define LOW_PAGE_LOOP2(a, label, C...)	do {			\
    uint32_t __n;						\
    for (__n = 0; __n < a->allocated; __n++) {			\
	ba_page p = a->pages[__n];				\
	while (p) {						\
	    ba_page t = p->hchain;				\
	    do { C; goto SURVIVE ## label; } while(0);		\
	    goto label; SURVIVE ## label: p = t;		\
	}							\
    }								\
label:								\
    0;								\
} while(0)
#endif
#define LOW_PAGE_LOOP(a, l, C...)	LOW_PAGE_LOOP2(a, l, C)
#define PAGE_LOOP(a, C...)	LOW_PAGE_LOOP(a, XCONCAT(page_loop_label, __LINE__), C)

#endif /* BLOCK_ALLOCATOR_H */

#ifdef __cplusplus
extern "C" {
#endif
#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <stdint.h>
#include <stddef.h>
#if defined(BA_MEMTRACE) || defined(BA_DEBUG)
# ifdef __cplusplus
#  include <cstdio>
# else
#  include <stdio.h>
# endif
#endif

#ifdef BA_USE_VALGRIND
# include <valgrind/valgrind.h>
# include <valgrind/memcheck.h>
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

/*#define BA_DEBUG*/

#ifdef HAS___BUILTIN_EXPECT
#define likely(x)	(__builtin_expect((x), 1))
#define unlikely(x)	(__builtin_expect((x), 0))
#else
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif
#define BA_ALLOC_INITIAL	8
/*#define BA_HASH_THLD		8 */
#define BA_MAX_EMPTY		8

#ifdef BA_DEBUG
#define IF_DEBUG(x)	x
#else
#define IF_DEBUG(x)
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
# ifdef __cplusplus
#  define ba_error(fmt, x...)	throw fmt
# else
#  include <stdio.h>
#  include <unistd.h>

static inline void __error(int line, char * file, char * msg) {
    fprintf(stderr, "%s:%d\t%s\n", file, line, msg);
    _exit(1);
}
#  define ba_error(x...)	do { snprintf(errbuf, 127, x); __error(__LINE__, __FILE__, errbuf); } while(0)

#  define BA_NEED_ERRBUF
extern char errbuf[];
# endif
#endif

#ifdef BA_DEBUG
# define BA_ERROR(x...)	do { fprintf(stderr, "ERROR at %s:%d\n", __FILE__, __LINE__); ba_print_htable(a); ba_show_pages(a); ba_error(x); } while(0)
#else
# define BA_ERROR(x...)	do { ba_error(x); } while(0)
#endif

#if SIZEOF_LONG == 8 || SIZEOF_LONG_LONG == 8
#define BV_LENGTH   64
#define BV_ONE	    ((uint64_t)1)
#define BV_NIL	    ((uint64_t)0)
#if SIZEOF_LONG == 8
#define BV_CLZ	    __builtin_clzl
#define BV_CTZ	    __builtin_ctzl
#else
#define BV_CLZ	    __builtin_clzll
#define BV_CTZ	    __builtin_ctzll
#endif
typedef uint64_t bv_int_t;
#else
#define BV_LENGTH   32
#define BV_ONE	    ((uint32_t)1)
#define BV_NIL	    ((uint32_t)0)
#define BV_CLZ	    __builtin_clz
#define BV_CTZ	    __builtin_ctz
typedef uint32_t bv_int_t;
#endif

#define BV_WIDTH    (BV_LENGTH/8)

struct bitvector {
    size_t length;
    bv_int_t * v;
};
#ifdef BA_DEBUG
static INLINE void bv_print(struct bitvector * bv);
#endif

static INLINE void bv_set_vector(struct bitvector * bv, void * p) {
    bv->v = (bv_int_t*)p;
}

static INLINE size_t bv_byte_length(struct bitvector * bv) {
    size_t bytes = (bv->length >> 3) + !!(bv->length & 7);
    if (bytes & (BV_LENGTH-1)) {
	bytes += (BV_LENGTH - (bytes & (BV_LENGTH-1)));
    }
    return bytes;
}

static INLINE void bv_set(struct bitvector * bv, size_t n, int value) {
    const size_t bit = n&(BV_LENGTH-1);
    const size_t c = n / BV_LENGTH;
    bv_int_t * _v = bv->v + c;
    if (value) *_v |= BV_ONE << bit;
    else *_v &= ~(BV_ONE << bit);
}

static INLINE int bv_get(struct bitvector * bv, size_t n) {
    const size_t bit = n&(BV_LENGTH-1);
    const size_t c = n / BV_LENGTH;
    return !!(bv->v[c] & (BV_ONE << bit));
}

static INLINE size_t bv_ctz(struct bitvector * bv, size_t n) {
    size_t bit = n&(BV_LENGTH-1);
    size_t c = n / BV_LENGTH;
    bv_int_t * _v = bv->v + c;
    bv_int_t V = *_v & (~BV_NIL << bit);

#ifdef BA_DEBUG
    bv_print(bv);
#endif

    bit = c * BV_LENGTH;
    while (!(V)) {
	if (bit >= bv->length) {
	    bit = (size_t)-1;
	    goto RET;
	}
	V = *(++_v);
	bit += (BV_WIDTH*8);
    }
    bit += BV_CTZ(V);
    if (bit >= bv->length) bit = (size_t)-1;

RET:
    return bit;
}

#ifdef BA_DEBUG
static INLINE void bv_print(struct bitvector * bv) {
    size_t i;
    for (i = 0; i < bv->length; i++) {
	fprintf(stderr, "%d", bv_get(bv, i));
    }
    fprintf(stderr, "\n");
}
#endif

struct ba_block_header {
    struct ba_block_header * next;
};

#define BA_ONE	((struct ba_block_header *)1)

struct ba_page_header {
    struct ba_block_header * first;
    uint32_t used;
    uint32_t flags;
};

#define BA_INIT_HEADER()    { NULL, 0, 0 }

#define BA_FLAG_SORTED	1

struct ba_page {
    struct ba_page_header h;
    struct ba_page *next, *prev;
    struct ba_page *hchain;
};

/*
 * every allocator has one of these, which describe the layout of pages
 * in a way that makes calculations easy
 */

struct ba_layout {
    size_t offset;
    uint32_t block_size;
    uint32_t blocks;
};

#define BA_INIT_LAYOUT(block_size, blocks) { 0, block_size, blocks }

struct ba_block_header * ba_sort_list(const struct ba_page *,
				      struct ba_block_header *,
				      const struct ba_layout *);

typedef struct ba_page * ba_p;
typedef struct ba_block_header * ba_b;

/* we assume here that malloc has sizeof(void*) bytes of overhead */
#define MALLOC_OVERHEAD (sizeof(void*))


#ifdef BA_STATS
#include <malloc.h>
struct ba_stats {
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
    "" name/*st_name*/,\
    0,0,0,0,0,0,0,\
    0,0}
#else
# define BA_INIT_STATS(block_size, blocks, name)
#endif

struct block_allocator {
    struct ba_layout l;
    struct ba_page_header h;
    ba_p alloc; /* current page used for allocation */
    ba_p last_free;
    ba_p first; /* doube linked list of other pages (!free,!full,!alloc) */
    ba_p * pages;
    ba_p empty; /* single linked list of empty pages */
    uint32_t magnitude;
    uint32_t empty_pages;
    uint32_t max_empty_pages;
    uint32_t allocated;
    uint32_t num_pages;
    char * blueprint;
#ifdef BA_STATS
    struct ba_stats stats;
#endif
};

typedef struct block_allocator block_allocator;
#define BA_INIT(block_size, blocks, name...) {\
    BA_INIT_LAYOUT(block_size, blocks),\
    BA_INIT_HEADER(),\
    NULL/*alloc*/,\
    NULL/*last_free*/,\
    NULL/*first*/,\
    NULL/*pages*/,\
    NULL/*empty*/,\
    0/*magnitude*/,\
    0/*empty_pages*/,\
    BA_MAX_EMPTY/*max_empty_pages*/,\
    0/*allocated*/,\
    0/*num_pages*/,\
    NULL/*blueprint*/,\
    BA_INIT_STATS(block_size, blocks, name)\
}

EXPORT void ba_low_free(struct block_allocator * a,
				    ba_p p, ba_b ptr);
EXPORT void ba_show_pages(const struct block_allocator * a);
EXPORT void ba_init(struct block_allocator * a, uint32_t block_size,
			 uint32_t blocks);
EXPORT void ba_low_alloc(struct block_allocator * a);
EXPORT void ba_find_page(struct block_allocator * a,
			 const void * ptr);
EXPORT void ba_remove_page(struct block_allocator * a);
#ifdef BA_DEBUG
EXPORT void ba_print_htable(const struct block_allocator * a);
EXPORT void ba_check_allocator(struct block_allocator * a, char*, char*, int);
#endif
EXPORT void ba_print_hashstats(const struct block_allocator * a);
#ifdef BA_STATS
EXPORT void ba_print_stats(struct block_allocator * a);
#endif
EXPORT void ba_free_all(struct block_allocator * a);
EXPORT size_t ba_count(const struct block_allocator * a);
EXPORT void ba_count_all(const struct block_allocator * a, size_t *num,
			 size_t *size);
EXPORT void ba_destroy(struct block_allocator * a);
EXPORT void ba_walk(struct block_allocator * a, void (*callback)(void*,void*,void*),
		    void * data);

#define BA_PAGE(a, n)   ((a)->pages[(n) - 1])
#define BA_BLOCKN(l, p, n) ((ba_b)(((char*)(p+1)) + (n)*((l).block_size)))
#if defined(BA_DEBUG) || !defined(BA_CRAZY)
# define BA_CHECK_PTR(l, p, ptr)	((p) && (size_t)((char*)ptr - (char*)(p)) <= (l).offset)
#else
# define BA_CHECK_PTR(l, p, ptr)	((size_t)((char*)ptr - (char*)(p)) <= (l).offset)
#endif
#define BA_LASTBLOCK(l, p) ((ba_b)((char*)(p) + (l).offset))

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

static INLINE size_t ba_capacity(const struct block_allocator * a) {
    return (size_t)a->num_pages * a->l.blocks;
}

static INLINE int ba_empty(struct ba_page_header * h) {
    return !(h->first);
}

static INLINE void ba_unshift(struct ba_page_header * h,
			      struct ba_block_header * b) {
    b->next = h->first;
    h->first = b;
    h->used --;
    if (b > b->next) h->flags &= ~BA_FLAG_SORTED;
}

ATTRIBUTE((malloc))
static INLINE struct ba_block_header * ba_shift(struct ba_page_header * h,
						const struct ba_page * p,
						const struct ba_layout * l) {
    struct ba_block_header * ptr = h->first;

#ifdef BA_USE_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(ptr, sizeof(void*));
#endif

    if (ptr->next == BA_ONE) {
	h->first = (ba_b)(((char*)ptr) + l->block_size);
#ifdef BA_USE_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED(h->first, sizeof(void*));
#endif
	h->first->next = (ba_b)(size_t)!(h->first == BA_LASTBLOCK(*l, p));
#ifdef BA_USE_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS(h->first, sizeof(void*));
#endif
	h->flags |= BA_FLAG_SORTED;
    } else
	h->first = ptr->next;

    h->used++;

    return ptr;
}


static INLINE uint32_t round_up32_(uint32_t v) {
    v |= v >> 1; /* first round down to one less than a power of 2 */
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v;
}

static INLINE uint32_t round_up32(const uint32_t v) {
    return round_up32_(v)+1;
}

static INLINE void ba_init_layout(struct ba_layout * l,
				  const uint32_t block_size,
				  const uint32_t blocks) {
    l->block_size = block_size;
    l->blocks = blocks;
    l->offset = sizeof(struct ba_page) + (blocks - 1) * block_size;
}

static INLINE void ba_align_layout(struct ba_layout * l) {
    /* overrun anyone ? ,) */
    uint32_t page_size = l->offset + l->block_size + MALLOC_OVERHEAD;
    if (page_size & (page_size - 1)) {
	page_size = round_up32(page_size);
    }
    page_size -= MALLOC_OVERHEAD + sizeof(struct ba_page);
    ba_init_layout(l, l->block_size, page_size/l->block_size);
}

ATTRIBUTE((malloc))
static INLINE void * ba_alloc(struct block_allocator * a) {
    ba_b ptr;
#ifdef BA_STATS
    struct ba_stats *s = &a->stats;
#endif

    if (ba_empty(&a->h)) {
	ba_low_alloc(a);
#ifdef BA_DEBUG
	ba_check_allocator(a, "after ba_low_alloc", __FILE__, __LINE__);
#endif
    }
#ifdef BA_USE_VALGRIND
    VALGRIND_MEMPOOL_ALLOC(a, a->h.first, a->l.block_size);
#endif

    ptr = ba_shift(&a->h, a->alloc, &a->l);

#ifdef BA_MEMTRACE
    PRINT("%% %p 0x%x\n", ptr, a->l.block_size);
#endif
#ifdef BA_STATS
    if (++s->st_used > s->st_max) {
      s->st_max = s->st_used;
      s->st_max_allocated = a->allocated;
      s->st_max_pages = a->num_pages;
      s->st_mallinfo = mallinfo();
    }
#endif
#ifdef BA_DEBUG
	ba_check_allocator(a, "after ba_alloc", __FILE__, __LINE__);
#endif

#ifdef BA_USE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED(ptr, a->l.block_size);
#endif
    
    return ptr;
}


ATTRIBUTE((always_inline))
static INLINE void ba_free(struct block_allocator * a, void * ptr) {
    struct ba_page * p;

#ifdef BA_MEMTRACE
    PRINT("%% %p\n", ptr);
#endif

#ifdef BA_DEBUG
    if (a->empty_pages == a->num_pages) {
	BA_ERROR("we did it!\n");
    }
#endif

#ifdef BA_STATS
    a->stats.st_used--;
#endif
    if (likely(BA_CHECK_PTR(a->l, a->alloc, ptr))) {
	INC(free_fast1);
	ba_unshift(&a->h, (struct ba_block_header*)ptr);
#ifdef BA_USE_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS(ptr, a->l.block_size);
	VALGRIND_MEMPOOL_FREE(a, ptr);
#endif
	return;
    }

    if (!BA_CHECK_PTR(a->l, a->last_free, ptr)) {
	ba_find_page(a, ptr);
    }

    p = a->last_free;

    ba_unshift(&p->h, (struct ba_block_header*)ptr);

    if ((p->h.used) && (((ba_b)ptr)->next)) {
	INC(free_fast2);
#ifdef BA_USE_VALGRIND
	VALGRIND_MEMPOOL_FREE(a, ptr);
#endif
	return;
    }

    ba_low_free(a, p, (ba_b)ptr);
}

#define MS(x)	#x

/* goto considered harmful */
#define LOW_PAGE_LOOP2(a, label, C...)	do {			\
    uint32_t __n;						\
    if (a->pages) for (__n = 0; __n < a->allocated; __n++) {	\
	ba_p p = a->pages[__n];					\
	while (p) {						\
	    ba_p t = p->hchain;					\
	    do { C; goto SURVIVE ## label; } while(0);		\
	    goto label; SURVIVE ## label: p = t;		\
	}							\
    }								\
label:								\
    __n = 0;							\
} while(0)
#define LOW_PAGE_LOOP(a, l, C...)	LOW_PAGE_LOOP2(a, l, C)
#define PAGE_LOOP(a, C...)	LOW_PAGE_LOOP(a, XCONCAT(page_loop_label, __LINE__), C)

/* 
 * =============================================
 * Here comes definitions for relocation strategies
 */

/*
 * void relocate_simple(void * p, void * stop, ptrdiff_t offset);
 *
 * all blocks from p up until < stop have been relocated. all internal
 * pointers have to be updated by adding offset.
 * this is used after realloc had to malloc new space.
 */

/*
 * void relocate_default(void * src, void * dst, size_t n)
 *
 * relocate n blocks from src to dst. dst is uninitialized.
 */

struct ba_relocation {
    void (*simple)(void*, void*, ptrdiff_t);
    void (*relocate)(void*, void*, size_t);
};

static INLINE void ba_simple_rel_pointer(void * ptr, ptrdiff_t diff) {
    char ** _ptr = (char**)ptr;
    if (*_ptr) *_ptr += diff;
}

/*
 * =============================================
 * Here comes definitions for local allocators.
 */

/* initialized as
 *
 */

#define BA_LINIT(block_size, blocks, max_blocks) {\
    BA_INIT_LAYOUT(block_size, blocks),\
    BA_INIT_HEADER(),\
    /**/NULL,\
    /**/NULL,\
    /**/max_blocks,\
    /**/NULL,\
    { NULL, NULL }\
}

struct ba_local {
    struct ba_layout l;
    struct ba_page_header h;
    ba_p page, last_free;
    struct block_allocator * a;
    struct ba_relocation rel;
    uint32_t max_blocks;
};

EXPORT void ba_init_local(struct ba_local * a, uint32_t block_size,
			  uint32_t blocks, uint32_t max_blocks,
			  void (*simple)(void*, void*, ptrdiff_t),
			  void (*relocate)(void*, void*, size_t));

EXPORT void ba_local_grow(struct ba_local * a, uint32_t blocks);
EXPORT void ba_local_get_page(struct ba_local * a);
EXPORT size_t ba_lcount(const struct ba_local * a);
EXPORT void ba_ldestroy(struct ba_local * a);
EXPORT void ba_lfree_all(struct ba_local * a);
EXPORT void ba_walk_local(struct ba_local * a,
			  void (*callback)(void*,void*,void*), void * data);

static INLINE size_t ba_lcapacity(const struct ba_local * a) {
    if (a->a) {
	return ba_capacity(a->a);
    } else {
	return (size_t)(a->page && a->l.blocks);
    }
}

/*
 * Guarentee that for a minimum of n allocations no relocation will be
 * triggered.
 */
static INLINE void ba_lreserve(struct ba_local * a, uint32_t n) {
    if (!a->a) {

	n += a->h.used;

	if (a->l.blocks < n) {
	    uint32_t blocks = a->l.blocks * 2;

	    while (blocks < n) blocks *= 2;

	    ba_local_grow(a, blocks);
	}
    }
}

ATTRIBUTE((malloc))
static INLINE void * ba_lalloc(struct ba_local * a) {
    ba_b ptr;

    if (ba_empty(&a->h))
	ba_local_get_page(a);
#ifdef BA_USE_VALGRIND
    VALGRIND_MEMPOOL_ALLOC(a, a->h.first, a->l.block_size);
#endif

    ptr = ba_shift(&a->h, a->page, &a->l);

#ifdef BA_USE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED(ptr, a->l.block_size);
#endif

    return ptr;
}

static INLINE void ba_lfree(struct ba_local * a, void * ptr) {
    struct ba_page * p;
    if (!a->a || BA_CHECK_PTR(a->l, a->page, ptr)) {
	ba_unshift(&a->h, (struct ba_block_header *)ptr);
#ifdef BA_USE_VALGRIND
	VALGRIND_MEMPOOL_FREE(a, ptr);
#endif
	return;
    }

    if (!BA_CHECK_PTR(a->l, a->last_free, ptr)) {
	ba_find_page(a->a, ptr);
	a->last_free = a->a->last_free;
    }

    p = a->last_free;

    ba_unshift(&p->h, (struct ba_block_header *)ptr);
#ifdef BA_USE_VALGRIND
    VALGRIND_MEMPOOL_FREE(a, ptr);
#endif

    if (!(p->h.used) || !(((ba_b)ptr)->next))
	ba_low_free(a->a, p, (ba_b)ptr);
}

/*
 * =============================================
 * Here comes definitions for shared allocators.
 */

#endif /* BLOCK_ALLOCATOR_H */
#ifdef __cplusplus
}
#endif

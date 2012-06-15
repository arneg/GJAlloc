#ifdef __cplusplus
extern "C" {
#endif
#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <stdint.h>
#include <stddef.h>
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

/*#define BA_DEBUG*/

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
/*#define BA_HASH_THLD		8 */
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

struct ba_block_header {
    struct ba_block_header * next;
#ifdef BA_DEBUG
    uint32_t magic;
#endif
};

struct ba_page {
    struct ba_block_header * first;
    struct ba_page *next, *prev;
    struct ba_page *hchain;
    uint32_t used;
};

typedef struct ba_page * ba_p;
typedef struct ba_block_header * ba_b;

/* we assume here that malloc has sizeof(void*) bytes of overhead */
#define BLOCK_HEADER_SIZE (sizeof(void*))


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
    "" name/*st_name*/,\
    0,0,0,0,0,0,0,\
    0,0}
#else
# define BA_INIT_STATS(block_size, blocks, name)
#endif

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

static INLINE void ba_init_layout(struct ba_layout * l,
				  const uint32_t block_size,
				  const uint32_t blocks) {
    l->block_size = block_size;
    l->blocks = blocks;
    l->offset = sizeof(struct ba_page) + (blocks - 1) * block_size;
}

static INLINE void ba_align_layout(struct ba_layout * l) {
    /* overrun anyone ? ,) */
    uint32_t page_size = l->offset + l->block_size + BLOCK_HEADER_SIZE;
    if (page_size & (page_size - 1)) {
	page_size = round_up32(page_size);
    }
    page_size -= BLOCK_HEADER_SIZE + sizeof(struct ba_page);
    ba_init_layout(l, l->block_size, page_size/l->block_size);
}

struct block_allocator {
    struct ba_layout l;
    ba_b free_blk; /* next free blk in alloc */
    ba_p alloc; /* current page used for allocating */
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
    struct block_alloc_stats stats;
#endif
};

typedef struct block_allocator block_allocator;
#define BA_INIT(block_size, blocks, name...) {\
    BA_INIT_LAYOUT(block_size, blocks),\
    NULL/*free_blk*/,\
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

#define BA_ONE	((ba_b)1)

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
#ifdef BA_STATS
EXPORT void ba_print_stats(struct block_allocator * a);
#endif
EXPORT void ba_free_all(struct block_allocator * a);
EXPORT void ba_count_all(struct block_allocator * a, size_t *num, size_t *size);
EXPORT void ba_destroy(struct block_allocator * a);

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

#ifdef BA_DEBUG
# define BA_MARK_FREED(ptr)  do {		    \
    ((ba_b)ptr)->magic = BA_MARK_FREE;		    \
} while(0)
# define BA_MARK_ALLOCED(ptr)  do {		    \
    ((ba_b)ptr)->magic = BA_MARK_ALLOC;		    \
} while(0)
#else
# define BA_MARK_FREED(ptr)
# define BA_MARK_ALLOCED(ptr)
#endif

ATTRIBUTE((malloc))
static INLINE void * ba_alloc(struct block_allocator * a) {
    ba_b ptr;
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
	a->free_blk = (ba_b)(((char*)ptr) + a->l.block_size);
	a->free_blk->next = (ba_b)(size_t)!(a->free_blk == BA_LASTBLOCK(a->l, a->alloc));
    } else
#endif
	a->free_blk = ptr->next;

    BA_MARK_ALLOCED(ptr);
#ifdef BA_MEMTRACE
    PRINT("%% %p 0x%x\n", ptr, a->block_size);
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
    
    return ptr;
}


ATTRIBUTE((always_inline))
static INLINE void ba_free(struct block_allocator * a, void * ptr) {
    ba_p p;

#ifdef BA_MEMTRACE
    PRINT("%% %p\n", ptr);
#endif

#ifdef BA_DEBUG
    if (a->empty_pages == a->num_pages) {
	BA_ERROR("we did it!\n");
    }
    if (((ba_b)ptr)->magic == BA_MARK_FREE) {
	BA_ERROR("double freed somethign\n");
    }
#endif
    BA_MARK_FREED(ptr);

#ifdef BA_STATS
    a->stats.st_used--;
#endif
    if (likely(BA_CHECK_PTR(a->l, a->alloc, ptr))) {
	INC(free_fast1);
	((ba_b)ptr)->next = a->free_blk;
	a->free_blk = (ba_b)ptr;
	return;
    }


    if (BA_CHECK_PTR(a->l, a->last_free, ptr)) {
	p = a->last_free;
	INC(free_fast2);
    } else {
	ba_find_page(a, ptr);
	p = a->last_free;
    }
    ((ba_b)ptr)->next = p->first;
    p->first = (ba_b)ptr;
    if ((--p->used) && (((ba_b)ptr)->next)) {
	INC(free_fast2);
	return;
    }

    ba_low_free(a, p, (ba_b)ptr);
}

#define MS(x)	#x

/* goto considered harmful */
#define LOW_PAGE_LOOP2(a, label, C...)	do {			\
    uint32_t __n;						\
    for (__n = 0; __n < a->allocated; __n++) {			\
	ba_p p = a->pages[__n];					\
	while (p) {						\
	    ba_p t = p->hchain;					\
	    do { C; goto SURVIVE ## label; } while(0);		\
	    goto label; SURVIVE ## label: p = t;		\
	}							\
    }								\
label:								\
    0;								\
} while(0)
#define LOW_PAGE_LOOP(a, l, C...)	LOW_PAGE_LOOP2(a, l, C)
#define PAGE_LOOP(a, C...)	LOW_PAGE_LOOP(a, XCONCAT(page_loop_label, __LINE__), C)

/* 
 * =============================================
 * Here comes definitions for relocation strategies
 */

/*
 * void relocate_simple(void * data, size_t n, ptrdiff_t offset);
 *
 * n blocks at data have been relocated. all internal pointers have to be
 * updated by adding offset.
 * this is used after realloc had to malloc new space.
 */

/*
 * void relocate_default(void * src, void * dst, size_t n)
 *
 * relocate n blocks from src to dst. dst is uninitialized.
 */

struct ba_relocation {
    void (*simple)(void *, size_t, ptrdiff_t);
    void (*relocate)(void*, void*, size_t);
};

/*
 * =============================================
 * Here comes definitions for local allocators.
 */

/* initialized as
 *
 */

#define BA_LINIT(block_size, blocks, max_blocks) {\
    BA_INIT_LAYOUT(block_size, blocks),\
    /**/NULL,\
    /**/NULL,\
    /**/0,\
    /**/max_blocks,\
    /**/NULL,\
    { NULL, NULL }\
}

struct ba_local {
    struct ba_layout l;
    ba_b free_block;
    ba_p page;
    uint32_t max_blocks;
    struct block_allocator * a;
    struct ba_relocation rel;
};

EXPORT void ba_init_local(struct ba_local * a, uint32_t block_size,
			  uint32_t blocks, uint32_t max_blocks,
			  void (*simple)(void *, size_t, ptrdiff_t),
			  void (*relocate)(void*, void*, size_t));

EXPORT void ba_local_get_page(struct ba_local * a);
EXPORT void ba_ldestroy(struct ba_local * a);
EXPORT void ba_lfree_all(struct ba_local * a);

ATTRIBUTE((malloc))
static INLINE void * ba_lalloc(struct ba_local * a) {
    ba_b ptr;

    if (!a->free_block) {
	ba_local_get_page(a);
    }

    ptr = a->free_block;

#ifndef BA_CHAIN_PAGE
    if (ptr->next == BA_ONE) {
	a->free_block = (ba_b)(((char*)ptr) + a->l.block_size);
	a->free_block->next = (ba_b)(size_t)!(a->free_block == BA_LASTBLOCK(a->l, a->page));
    } else
#endif
	a->free_block = ptr->next;

    BA_MARK_ALLOCED(ptr);

    return ptr;
}

static INLINE void ba_lfree(struct ba_local * a, void * ptr) {
    if (!a->a || BA_CHECK_PTR(a->l, a->page, ptr)) {
#if BA_DEBUG
	if (((ba_b)ptr)->magic == BA_MARK_FREE) {
	    ba_error("double free");
	}
#endif
	BA_MARK_FREED(ptr);
	((ba_b)ptr)->next = a->free_block;
	a->free_block = (ba_b)ptr;
    } else {
	ba_free(a->a, ptr);
    }
}

/*
 * =============================================
 * Here comes definitions for shared allocators.
 */

#endif /* BLOCK_ALLOCATOR_H */
#ifdef __cplusplus
}
#endif

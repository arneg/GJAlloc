#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* vim indenting workaround */
#endif
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#ifdef BA_STATS
# include <unistd.h>
# include <mcheck.h>
#endif

#include "gjalloc.h"

#ifdef BA_NEED_ERRBUF
EXPORT char errbuf[128];
#endif

static INLINE void ba_htable_insert(const struct block_allocator * a,
				    ba_p ptr);
static INLINE void ba_htable_grow(struct block_allocator * a);

#define BA_NBLOCK(l, p, ptr)	((uintptr_t)((char*)(ptr) - (char*)((p)+1))/(uintptr_t)((l).block_size))

#define BA_PAGESIZE(l)	    ((l).offset + (l).block_size)
#define BA_HASH_MASK(a)  (((a->allocated)) - 1)

#define BA_BYTES(a)	( (sizeof(ba_p) * ((a)->allocated) ) )

#define PRINT_NODE(a, name) do {\
    fprintf(stderr, #name": %p\n", a->name);\
} while (0)


EXPORT void ba_show_pages(const struct block_allocator * a) {
    fprintf(stderr, "allocated: %u\n", a->allocated);
    fprintf(stderr, "num_pages: %u\n", a->num_pages);
    fprintf(stderr, "max_empty_pages: %u\n", a->max_empty_pages);
    fprintf(stderr, "empty_pages: %u\n", a->empty_pages);
    fprintf(stderr, "magnitude: %u\n", a->magnitude);
    PRINT_NODE(a, empty);
    PRINT_NODE(a, first);
    PRINT_NODE(a, last_free);

    a->alloc->h = a->h;
    PAGE_LOOP(a, {
	uint32_t blocks_used;
	blocks_used = p->h.used;
	fprintf(stderr, "(%p)\t%f\t(%u %d) --> (prev: %p | next: %p)\n",
		p, blocks_used/(double)a->l.blocks * 100,
		blocks_used,
		blocks_used,
		p->prev, p->next);
    });
}

#ifdef BA_STATS
# define PRCOUNT(X)	fprintf(stat_file, #X " %5.1lf %% ", (a->stats.X/all)*100);
static FILE * stat_file = NULL;
ATTRIBUTE((constructor))
static void ba_stats_init() {
    char * fname = getenv("BA_STAT_TRACE");
    if (fname) {
	stat_file = fopen(fname, "w");
    } else stat_file = stderr;
}
ATTRIBUTE((destructor))
static void ba_stats_deinit() {
    if (stat_file != stderr) {
	fclose(stat_file);
	stat_file = stderr;
    }
}
EXPORT void ba_print_stats(struct block_allocator * a) {
    FILE *f;
    int i;
    double all;
    struct block_alloc_stats * s = &a->stats;
    size_t used = s->st_max * a->l.block_size;
    size_t malloced = s->st_max_pages * (a->l.block_size * a->l.blocks);
    size_t overhead = s->st_max_pages * sizeof(struct ba_page);
    int mall = s->st_mallinfo.uordblks;
    int moverhead = s->st_mallinfo.fordblks;
    const char * fmt = "%s:\n max used %.1lf kb in %.1lf kb (#%lld) pages"
		     " (overhead %.2lf kb)"
		     " mall: %.1lf kb overhead: %.1lf kb "
		     " page_size: %d block_size: %d\n";
    overhead += s->st_max_allocated * 2 * sizeof(void*);
    if (s->st_max == 0) return;
    fprintf(stat_file, fmt,
	   s->st_name,
	   used / 1024.0, malloced / 1024.0,
	   s->st_max_pages, overhead / 1024.0,
	   mall / 1024.0,
	   moverhead / 1024.0,
	   a->l.block_size * a->l.blocks,
	   a->l.block_size
	   );
    all = ((a->stats.free_fast1 + a->stats.free_fast2 + a->stats.find_linear + a->stats.find_hash + a->stats.free_empty + a->stats.free_full));
    if (all == 0.0) return;
    fprintf(stat_file, "free COUNTS: ");
    PRCOUNT(free_fast1);
    PRCOUNT(free_fast2);
    PRCOUNT(find_linear);
    PRCOUNT(find_hash);
    PRCOUNT(free_empty);
    PRCOUNT(free_full);
    fprintf(stat_file, " all %lu\n", (long unsigned int)all);
}
#endif


#ifndef BA_CMEMSET
static INLINE void cmemset(char * dst, const char * src, size_t s,
			   size_t n) {
    memcpy(dst, src, s);

    for (--n,n *= s; n >= s; n -= s,s <<= 1)
	memcpy(dst + s, dst, s);

    if (n) memcpy(dst + s, dst, n);
}
#define BA_CMEMSET(dst, src, s, n)	cmemset(dst, src, s, n)
#endif

#ifndef BA_XALLOC
ATTRIBUTE((malloc))
static INLINE void * ba_xalloc(const size_t size) {
    void * p = malloc(size);
    if (!p) ba_error("no mem");
    return p;
}
#define BA_XALLOC(size)	ba_xalloc(size)
#endif

#ifndef BA_XREALLOC
ATTRIBUTE((malloc))
static INLINE void * ba_xrealloc(void * p, const size_t size) {
    p = realloc(p, size);
    if (!p) ba_error("no mem");
    return p;
}
#define BA_XREALLOC(ptr, size)	ba_xrealloc(ptr, size)
#endif

EXPORT void ba_init(struct block_allocator * a, uint32_t block_size,
		    uint32_t blocks) {

    if (!block_size) BA_ERROR("ba_init called with zero block_size\n");

    if (block_size < sizeof(struct ba_block_header)) {
	block_size = sizeof(struct ba_block_header);
    }

    if (!blocks) blocks = 512;

    ba_init_layout(&a->l, block_size, blocks);
    ba_align_layout(&a->l);

    a->empty = a->first = NULL;
    a->last_free = NULL;
    a->alloc = NULL;
    a->h.first = NULL;

#ifdef BA_DEBUG
    fprintf(stderr, "blocks: %u block_size: %u page_size: %u\n",
	    blocks, block_size, BA_PAGESIZE(a->l));
#endif


#ifndef ctz32
# define ctz32 __builtin_ctz
#endif
    a->magnitude = ctz32(round_up32(BA_PAGESIZE(a->l)));
    a->num_pages = 0;
    a->empty_pages = 0;
    a->max_empty_pages = BA_MAX_EMPTY;

    /* we start with management structures for BA_ALLOC_INITIAL pages */
    a->allocated = BA_ALLOC_INITIAL;
    a->pages = (ba_p*)BA_XALLOC(BA_ALLOC_INITIAL * sizeof(ba_p));
    memset(a->pages, 0, BA_ALLOC_INITIAL * sizeof(ba_p));
    a->blueprint = NULL;
#ifdef BA_STATS
    memset(&a->stats, 0, sizeof(struct ba_stats));
#endif
#ifdef BA_USE_VALGRIND
    VALGRIND_CREATE_MEMPOOL(a, 0, 0);
#endif

#ifdef BA_DEBUG
    fprintf(stderr, " -> blocks: %u block_size: %u page_size: %u mallocing size: %u, next pages: %u magnitude: %u\n",
	    a->l.blocks, block_size, BA_PAGESIZE(a->l)-sizeof(struct ba_page),
	    BA_PAGESIZE(a->l),
	    round_up32(BA_PAGESIZE(a->l)),
	    a->magnitude);
#endif
}

static INLINE void ba_free_page(const struct ba_layout * l,
				struct ba_page * p,
				const void * blueprint) {
    const char * bp = (char*)blueprint;
    p->h.first = BA_BLOCKN(*l, p, 0);
    p->h.used = 0;
    p->h.flags = BA_FLAG_SORTED;

    if (bp)
	BA_CMEMSET((char*)(p+1), bp, l->block_size, l->blocks);

    p->h.first->next = BA_ONE;
#ifdef BA_USE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS(p+1, l->block_size * l->blocks);
#endif
}

EXPORT INLINE void ba_free_all(struct block_allocator * a) {
    if (!a->allocated) return;

    PAGE_LOOP(a, {
	free(p);
    });

    a->h.first = NULL;
    a->alloc = NULL;
    a->num_pages = 0;
    a->empty_pages = 0;
    a->empty = a->first = NULL;
    a->last_free = NULL;
    a->allocated = BA_ALLOC_INITIAL;
    memset(a->pages, 0, a->allocated * sizeof(ba_p));
}

EXPORT void ba_count_all(const struct block_allocator * a, size_t *num,
			 size_t *size) {
    /* fprintf(stderr, "page_size: %u, pages: %u\n", BA_PAGESIZE(a->l), a->num_pages); */
    *size = BA_BYTES(a) + a->num_pages * BA_PAGESIZE(a->l);
    *num = ba_count(a);
}

EXPORT size_t ba_count(const struct block_allocator * a) {
    size_t n = 0;

    if (a->alloc) a->alloc->h = a->h;

    PAGE_LOOP(a, {
	n += p->h.used;
    });
    return n;
}

EXPORT void ba_destroy(struct block_allocator * a) {
    PAGE_LOOP(a, {
	free(p);
    });

    a->h.first = NULL;
    a->alloc = NULL;
    free(a->pages);
    a->last_free = NULL;
    a->allocated = 0;
    a->pages = NULL;
    a->empty = a->first = NULL;
    a->empty_pages = 0;
    a->num_pages = 0;
#ifdef BA_STATS
    a->stats.st_max = a->stats.st_used = 0;
#endif
#ifdef BA_USE_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(a);
#endif
}

static INLINE void ba_grow(struct block_allocator * a) {
    if (a->allocated) {
	/* try to detect 32bit overrun? */
	if (a->allocated >= ((uint32_t)1 << (sizeof(uint32_t)*8-1))) {
	    BA_ERROR("too many pages.\n");
	}
	ba_htable_grow(a);
    } else {
	ba_init(a, a->l.block_size, a->l.blocks);
    }
#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_grow", __FILE__, __LINE__);
#endif
}

#define MIX(t)	do {		\
    t ^= (t >> 20) ^ (t >> 12);	\
    t ^= (t >> 7) ^ (t >> 4);	\
} while(0)

static INLINE uint32_t hash1(const struct block_allocator * a,
			     const void * ptr) {
    uintptr_t t = ((uintptr_t)ptr) >> a->magnitude;

    MIX(t);

    return (uint32_t) t;
}

static INLINE uint32_t hash2(const struct block_allocator * a,
			     const void * ptr) {
    uintptr_t t = ((uintptr_t)ptr) >> a->magnitude;

    t++;
    MIX(t);

    return (uint32_t) t;
}

static INLINE void ba_htable_grow(struct block_allocator * a) {
    uint32_t n, old;
    old = a->allocated;
    a->allocated *= 2;
    a->pages = (ba_p*)BA_XREALLOC(a->pages, a->allocated * sizeof(ba_p));
    memset(a->pages+old, 0, old * sizeof(ba_p));
    for (n = 0; n < old; n++) {
	ba_p * b = a->pages + n;

	while (*b) {
	    uint32_t h = hash1(a, BA_LASTBLOCK(a->l, *b)) & BA_HASH_MASK(a);
	    if (h != n) {
		ba_p p = *b;
		*b = p->hchain;
		p->hchain = a->pages[h];
		a->pages[h] = p;
		continue;
	    }
	    b = &(*b)->hchain;
	}
    }
}

static INLINE void ba_htable_shrink(struct block_allocator * a) {
    uint32_t n;

    a->allocated /= 2;

    for (n = 0; n < a->allocated; n++) {
	ba_p p = a->pages[a->allocated + n];

	if (p) {
	    ba_p t = p;
	    if (a->pages[n]) {
		while (t->hchain) t = t->hchain;
		t->hchain = a->pages[n];
	    }
	    a->pages[n] = p;
	}
    }

    a->pages = (ba_p*)BA_XREALLOC(a->pages, a->allocated * sizeof(ba_p));
}

#ifdef BA_DEBUG
EXPORT void ba_print_htable(const struct block_allocator * a) {
    unsigned int i;

    for (i = 0; i < a->allocated; i++) {
	ba_p p = a->pages[i];
	uint32_t hval;
	if (!p) {
	    fprintf(stderr, "empty %u\n", i);
	}
	while (p) {
	    void * ptr = BA_LASTBLOCK(a->l, p);
	    hval = hash1(a, ptr);
	    fprintf(stderr, "%u\t%X[%u]\t%p-%p\t%X\n", i, hval, hval & BA_HASH_MASK(a), p+1, ptr, (unsigned int)((uintptr_t)ptr >> a->magnitude));
	    p = p->hchain;
	}
    }
}
#endif

/*
 * insert the pointer to an allocated page into the
 * hashtable.
 */
static INLINE void ba_htable_insert(const struct block_allocator * a,
				    ba_p p) {
    uint32_t hval = hash1(a, BA_LASTBLOCK(a->l, p));
    ba_p * b = a->pages + (hval & BA_HASH_MASK(a));


#ifdef BA_DEBUG
    while (*b) {
	if (*b == p) {
	    fprintf(stderr, "inserting (%p) twice\n", p);
	    fprintf(stderr, "is (%p)\n", *b);
	    BA_ERROR("double insert.\n");
	    return;
	}
	b = &(*b)->hchain;
    }
    b = a->pages + (hval & BA_HASH_MASK(a));
#endif

#if 0/*def BA_DEBUG */
    fprintf(stderr, "replacing bucket %u with page %u by %u\n",
	    hval & BA_HASH_MASK(a), *b, n);
#endif

    p->hchain = *b;
    *b = p;
}


#if 0
static INLINE void ba_htable_replace(const struct block_allocator * a, 
				     const void * ptr, const uint32_t n,
				     const uint32_t new) {
    uint32_t hval = hash1(a, ptr);
    uint32_t * b = a->htable + (hval & BA_HASH_MASK(a));

    while (*b) {
	if (*b == n) {
	    *b = new;
	    BA_PAGE(a, new)->hchain = BA_PAGE(a, n)->hchain;
	    BA_PAGE(a, n)->hchain = 0;
	    return;
	}
	b = &BA_PAGE(a, *b)->hchain;
    }
#ifdef DEBUG

    fprintf(stderr, "ba_htable_replace(%p, %u, %u)\n", ptr, n, new);
    fprintf(stderr, "hval: %u, %u, %u\n", hval, hval & BA_HASH_MASK(a), BA_HASH_MASK(a));
    ba_print_htable(a);
    BA_ERROR("did not find index to replace.\n")
#endif
}
#endif

static INLINE void ba_htable_delete(const struct block_allocator * a,
				    ba_p p) {
    uint32_t hval = hash1(a, BA_LASTBLOCK(a->l, p));
    ba_p * b = a->pages + (hval & BA_HASH_MASK(a));

    while (*b) {
	if (*b == p) {
	    *b = p->hchain;
	    p->hchain = 0;
	    return;
	}
	b = &(*b)->hchain;
    }
#ifdef DEBUG
    ba_print_htable(a);
    fprintf(stderr, "ba_htable_delete(%p, %u)\n", p, n);
    BA_ERROR("did not find index to delete.\n")
#endif
}

static INLINE void ba_htable_lookup(struct block_allocator * a,
				    const void * ptr) {
    ba_p p;
    uint32_t h[2];
    unsigned char c, b = 0;
    h[0] = hash1(a, ptr);
    h[1] = hash2(a, ptr);

    c = ((uintptr_t)ptr >> (a->magnitude - 1)) & 1;

LOOKUP:
    p = a->pages[h[c] & BA_HASH_MASK(a)];
    while (p) {
	if (BA_CHECK_PTR(a->l, p, ptr)) {
	    a->last_free = p;
	    return;
	}
	p = p->hchain;
    }
    c = !c;
    if (!(b++)) goto LOOKUP;
}

#ifdef BA_DEBUG
EXPORT INLINE void ba_check_allocator(struct block_allocator * a,
				    char *fun, char *file, int line) {
    uint32_t n;
    int bad = 0;
    ba_p p;

    if (a->alloc)
	a->alloc->h = a->h;

    if (a->empty_pages > a->num_pages) {
	fprintf(stderr, "too many empty pages.\n");
	bad = 1;
    }

    for (n = 0; n < a->allocated; n++) {
	uint32_t hval;
	p = a->pages[n];

	while (p) {
	    if (p != a->alloc) {
		if (p->prev || p->next) {
		    fprintf(stderr, "page %p is full but in list. next: %p"
			     "prev: %p\n",
			    p, p->next,
			    p->prev);
		    bad = 1;
		}
	    }

	    hval = hash1(a, BA_LASTBLOCK(a->l, p)) & BA_HASH_MASK(a);

	    if (hval != n) {
		fprintf(stderr, "page with hash %u found in bucket %u\n",
			hval, n);
		bad = 1;
	    }
	    p = p->hchain;
	}

	if (bad)
	    ba_print_htable(a);
    }

    if (bad) {
	ba_show_pages(a);
	fprintf(stderr, "\nCalled from %s:%d:%s\n", fun, line, file);
	fprintf(stderr, "pages: %u\n", a->num_pages);
	BA_ERROR("bad");
    }
}
#endif

static INLINE ba_p ba_alloc_page(struct block_allocator * a) {
    ba_p p;

    if (unlikely(a->num_pages == a->allocated)) {
	ba_grow(a);
    }

    a->num_pages++;
    p = (ba_p)BA_XALLOC(BA_PAGESIZE(a->l));
    ba_free_page(&a->l, p, a->blueprint);
    p->next = p->prev = NULL;
    ba_htable_insert(a, p);
#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_alloc after insert.", __FILE__, __LINE__);
#endif

    return p;
}

static INLINE ba_p ba_get_page(struct block_allocator * a) {
    ba_p p;

    if (a->first) {
	p = a->first;
	DOUBLE_SHIFT(a->first);
    } else if (a->empty) {
	p = a->empty;
	SINGLE_SHIFT(a->empty);
	a->empty_pages--;
    } else {
	p = ba_alloc_page(a);
    }
    p->next = NULL;

    return p;
}

EXPORT void ba_low_alloc(struct block_allocator * a) {
    /* fprintf(stderr, "ba_alloc(%p)\n", a); */
#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_alloc top", __FILE__, __LINE__);
#endif
    if (a->alloc) {
	a->alloc->h = a->h;
    }

    a->alloc = ba_get_page(a);
    a->h = a->alloc->h;

#ifdef BA_DEBUG
    if (!a->h.first) {
	ba_show_pages(a);
	BA_ERROR("a->first has no first block!\n");
    }
#endif

#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_alloc after grow", __FILE__, __LINE__);
#endif
}

EXPORT void ba_low_free(struct block_allocator * a, ba_p p, ba_b ptr) {
#ifdef BA_USE_VALGRIND
    VALGRIND_MEMPOOL_FREE(a, ptr);
    VALGRIND_MAKE_MEM_NOACCESS(ptr, a->l.block_size);
#endif
    if (!p->h.used) {
	INC(free_empty);
	DOUBLE_UNLINK(a->first, p);
	/* reset the internal list. this avoids fragmentation which would otherwise
	 * happen when reusing pages. Since that is cheap here, we do it.
	 */
	p->h.first = BA_BLOCKN(a->l, p, 0);
#ifdef BA_USE_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED(a->h.first, sizeof(void*));
#endif
	p->h.first->next = BA_ONE;
#ifdef BA_USE_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS(a->h.first, sizeof(void*));
#endif
	SINGLE_LINK(a->empty, p);
	a->empty_pages ++;
	/*
	 * This comparison looks strange, however the first page never gets
	 * freed, so this is actually ok.
	 */
	if (a->empty_pages >= a->max_empty_pages) {
	    ba_remove_page(a);
	    return;
	}
    } else { /* page was full */
	INC(free_full);
	DOUBLE_LINK(a->first, p);
    }
}

static INLINE void ba_htable_linear_lookup(struct block_allocator * a,
				    const void * ptr) {
    PAGE_LOOP(a, {
	if (BA_CHECK_PTR(a->l, p, ptr)) {
	    a->last_free = p;
	    return;
	}
    });
}

EXPORT void ba_find_page(struct block_allocator * a,
				     const void * ptr) {
    a->last_free = NULL;
#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_low_free", __FILE__, __LINE__);
#endif
#ifdef BA_HASH_THLD
    if (a->num_pages <= BA_HASH_THLD) {
	INC(find_linear);
	ba_htable_linear_lookup(a, ptr);
    } else {
#endif
	INC(find_hash);
	ba_htable_lookup(a, ptr);
#ifdef BA_HASH_THLD
    }
#endif
    if (a->last_free) return;
    
#ifdef BA_DEBUG
    fprintf(stderr, "magnitude: %u\n", a->magnitude);
    fprintf(stderr, "allocated: %u\n", a->allocated);
    fprintf(stderr, "did not find %p (%X[%X] | %X[%X])\n", ptr,
	    hash1(a, ptr), hash1(a, ptr) & BA_HASH_MASK(a),
	    hash2(a, ptr), hash2(a, ptr) & BA_HASH_MASK(a)
	    );
    ba_print_htable(a);
#endif
    BA_ERROR("Unknown pointer (not found in hash) %lx\n", (long int)ptr);
}

EXPORT void ba_remove_page(struct block_allocator * a) {
    ba_p p;
    ba_p * c = &(a->empty), * max = c;

    while (*c) {
	if (*c > *max) {
	    max = c;
	}
	c = &((*c)->next);
    }

    p = *max;
    *max = (*max)->next;

#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_remove_page", __FILE__, __LINE__);
    if (a->empty_pages < a->max_empty_pages) {
	BA_ERROR("strange things happening\n");
    }
    if (p == a->first) {
	BA_ERROR("removing first page. this is not supposed to happen\n");
    }
#endif

#if 0/*def BA_DEBUG*/
    fprintf(stderr, "> ===============\n");
    ba_show_pages(a);
    ba_print_htable(a);
    fprintf(stderr, "removing page %4u[%p]\t(%p .. %p) -> %X (%X) (of %u).\n", 
	    n, p, p+1, BA_LASTBLOCK(a->l, p), hash1(a, BA_LASTBLOCK(a->l,p)),
	    hash1(a, BA_LASTBLOCK(a->l,p)) & BA_HASH_MASK(a),
	    a->num_pages);
#endif

    ba_htable_delete(a, p);

    /* we know that p == a->last_free */
    a->last_free = NULL;

#ifdef BA_DEBUG
    memset(p, 0, sizeof(struct ba_page));
    if (a->first == p) {
	fprintf(stderr, "a->first will be old removed page %p\n", a->first);
    }
#endif

    a->empty_pages--;
    a->num_pages--;
    free(p);

#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_remove_page", __FILE__, __LINE__);
#endif
    if (a->allocated > BA_ALLOC_INITIAL && a->num_pages <= (a->allocated >> 2)) {
	ba_htable_shrink(a);
    }
}

/*
 * Here come local allocator definitions
 */

EXPORT size_t ba_lcount(const struct ba_local * a) {
    if (a->a) return ba_count(a->a);
    else return (size_t)(a->page && a->h.used);
}

EXPORT void ba_init_local(struct ba_local * a, uint32_t block_size,
			  uint32_t blocks, uint32_t max_blocks,
			  void (*simple)(void *, void*, ptrdiff_t),
			  void (*relocate)(void*, void*, size_t)) {

    if (block_size < sizeof(struct ba_block_header)) {
	block_size = sizeof(struct ba_block_header);
    }

    if (!blocks) blocks = 16;
    if (!max_blocks) max_blocks = 512;

    if (blocks >= max_blocks) {
	ba_error("foo");
    }

    if (!simple) {
	ba_error("simple relocation mechanism needs to be implemented");
    }

    ba_init_layout(&a->l, block_size, blocks);
    a->h.first = NULL;
    a->page = (ba_p)NULL;
    a->max_blocks = max_blocks;
    a->a = NULL;
    a->rel.simple = simple;
    a->rel.relocate = relocate;
#ifdef BA_USE_VALGRIND
    VALGRIND_CREATE_MEMPOOL(a, 0, 0);
#endif
}

static INLINE struct ba_page * ba_local_grow_page(struct ba_page * p,
					   struct ba_page_header * h,
					   const struct ba_layout * l,
					   const struct ba_relocation * rel) {

    struct ba_block_header * stop;
    ptrdiff_t diff;
    struct ba_page * n;

    n = (ba_p)BA_XREALLOC(p, BA_PAGESIZE(*l));
#ifdef BA_USE_VALGRIND
    /* l is in principle different from the allocator, however
     * as long as we keep it as the first item in the struct,
     * its fine to use it
     */
    VALGRIND_MEMPOOL_CHANGE(l, p, n, BA_PAGESIZE(*l));
#endif
    stop = BA_BLOCKN(*l, n, h->used);
    diff = (char*)n - (char*)p;
    if (diff) {
	rel->simple(BA_BLOCKN(*l, n, 0), stop, diff);
    }
    h->first = stop;
    h->first->next = BA_ONE;

#ifdef BA_USE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS(stop, l->block_size*(l->blocks - h->used));
#endif

    return n;
}

EXPORT INLINE void ba_local_grow(struct ba_local * a,
				 const uint32_t blocks) {
    struct ba_layout l;
    int transform = 0;

    if (blocks >= a->max_blocks) {
	ba_init_layout(&l, a->l.block_size, a->max_blocks);
	ba_align_layout(&l);
	transform = 1;
    } else {
	ba_init_layout(&l, a->l.block_size, blocks);
    }

    a->page = ba_local_grow_page(a->page, &a->h, &l, &a->rel);
    a->l = l;

    if (transform) {
#ifdef BA_DEBUG
	fprintf(stderr, "transforming into allocator\n");
#endif
	a->a = (struct block_allocator *)
		BA_XALLOC(sizeof(struct block_allocator));
	ba_init(a->a, l.block_size, l.blocks);
	/* transform here! */
	a->page->next = a->page->prev = NULL;
	a->a->num_pages ++;
	//a->a->first = a->page;
	ba_htable_insert(a->a, a->page);
    }
}


EXPORT void ba_local_get_page(struct ba_local * a) {
    if (a->a) {
	/* old page is full */
	if (a->page) {
	    a->page->h = a->h;
	}
	/* get page from allocator */
	a->page = ba_get_page(a->a);
	a->h = a->page->h;
    } else {
	if (a->page) {
	    /* double the size */
	    ba_local_grow(a, a->l.blocks*2);
	} else {
	    ba_init_layout(&a->l, a->l.block_size, a->l.blocks);
	    a->page = (ba_p)BA_XALLOC(BA_PAGESIZE(a->l));
	    ba_free_page(&a->l, a->page, NULL);
	    a->h = a->page->h;
	}
    }
}

EXPORT void ba_ldestroy(struct ba_local * a) {
    if (a->a) {
	ba_destroy(a->a);
	free(a->a);
	a->a = NULL;
    } else {
	free(a->page);
    }
    a->page = NULL;
    a->h.first = NULL;
#ifdef BA_USE_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(a);
#endif
}

EXPORT void ba_lfree_all(struct ba_local * a) {
    if (a->a) {
	ba_free_all(a->a);
    } else {
	/* could be only in a->h */
	ba_free_page(&a->l, a->page, NULL);
	a->h = a->page->h;
    }
}

static INLINE void ba_list_defined(const struct ba_page * p,
				   const struct ba_block_header * b,
				   const struct ba_layout * l) {
#ifdef BA_USE_VALGRIND
    struct ba_page_header h;
    h.first = (struct ba_block_header *)b;
    while (h.first) {
	VALGRIND_MAKE_MEM_DEFINED(h.first, sizeof(void*));
	ba_shift(&h, p, l);
    }
#endif
}

static INLINE void ba_list_noaccess(const struct ba_page * p,
				    const struct ba_block_header * b,
				    const struct ba_layout * l) {
#ifdef BA_USE_VALGRIND
    struct ba_page_header h;
    h.first = (struct ba_block_header *)b;
    while (h.first) {
	b = h.first;
	ba_shift(&h, p, l);
	VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(void*));
    }
#endif
}

void ba_walk_page(const struct ba_layout * l,
		  struct ba_page * p,
		  void (*callback)(void*,void*,void*),
		  void * data) {
    const struct ba_block_header * b;
    char * start;

#define BA_CALLBACK(a, b, c)	do {	\
    ba_list_noaccess(p, b, l);		\
    callback(a, b, c);			\
    ba_list_defined(p, b, l);		\
} while (0)

    if (!(p->h.flags & BA_FLAG_SORTED)) {
	p->h.first = ba_sort_list(p, p->h.first, l);
	p->h.flags |= BA_FLAG_SORTED;
    }
    b = p->h.first;

    ba_list_defined(p, b, l);

    if (!b) {
	/* page is full, walk all blocks */
	start = (char*)BA_BLOCKN(*l, p, 0);
	goto walk_rest;
    }

    if (b != BA_BLOCKN(*l, p, 0)) {
	BA_CALLBACK(BA_BLOCKN(*l, p, 0), (void*)b, data);
    }

    /*
     * TODO:
     *
     */

    do {
	struct ba_block_header * n = b->next;
	start = (char*)b + l->block_size;
	if (n == BA_ONE) return;
	if (!n) break;
	if ((char*)n > start) {
	    BA_CALLBACK(start, n, data);
	}
	b = n;
    } while (b);

    if (start <= (char*)BA_LASTBLOCK(*l, p)) {
walk_rest:
	BA_CALLBACK(start, BA_LASTBLOCK(*l, p) + l->block_size, data);
    }

    ba_list_noaccess(p, b, l);

}

EXPORT void ba_walk(struct block_allocator * a,
	     void (*callback)(void*,void*,void*),
	     void * data) {
    if (a->alloc)
	a->alloc->h = a->h;

    PAGE_LOOP(a, {
	if (p->h.used)
	    ba_walk_page(&a->l, p, callback, data);
    });

    if (a->alloc)
	a->h = a->alloc->h;
}

EXPORT void ba_walk_local(struct ba_local * a,
	     void (*callback)(void*,void*,void*),
	     void * data) {
    if (a->page)
	a->page->h = a->h;
    if (!a->a) {
	if (a->page)
	    ba_walk_page(&a->l, a->page, callback, data);
    } else ba_walk(a->a, callback, data);
    if (a->page)
	a->h = a->page->h;
}


struct ba_block_header * ba_sort_list(const struct ba_page * p,
				      struct ba_block_header * b,
				      const struct ba_layout * l) {
    struct bitvector v;
    size_t i, j;
    struct ba_block_header ** t = &b;

    ba_list_defined(p, b, l);

#ifdef BA_DEBUG
    fprintf(stderr, "sorting max %llu blocks\n",
	    (unsigned long long)l->blocks);
#endif
    v.length = l->blocks;
    i = bv_byte_length(&v);
    /* we should probably reuse an area for this.
     */
    bv_set_vector(&v, BA_XALLOC(i));
    memset(v.v, 0, i);

    /*
     * store the position of all blocks in a bitmask
     */
    while (b) {
	uintptr_t n = BA_NBLOCK(*l, p, b);
#ifdef BA_DEBUG
	fprintf(stderr, "block %llu is free\n", (long long unsigned)n);
#endif
	bv_set(&v, n, 1);
	if (b->next == BA_ONE) {
	    v.length = n+1;
	    break;
	} else b = b->next;
    }

#ifdef BA_DEBUG
    bv_print(&v);
#endif

    /*
     * Handle consecutive free blocks in the end, those
     * we dont need anyway.
     */
    if (v.length) {
	i = v.length-1;
	while (i && bv_get(&v, i)) { i--; }
	v.length = i+1;
    }

#ifdef BA_DEBUG
    bv_print(&v);
#endif

    j = 0;

    /*
     * We now rechain all blocks.
     */
    while ((i = bv_ctz(&v, j)) != (size_t)-1) {
	if (i < j) return b;
	*t = BA_BLOCKN(*l, p, i);
	t = &((*t)->next);
	j = i+1;
    }

    /*
     * The last one
     */

    if (v.length < l->blocks) {
	*t = BA_BLOCKN(*l, p, v.length);
	(*t)->next = BA_ONE;
    } else (*t)->next = NULL;

    free(v.v);

    ba_list_noaccess(p, b, l);

    return b;
}


#ifdef __cplusplus
}
#endif

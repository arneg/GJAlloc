#ifdef __cplusplus
extern "C" {
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

#define BA_NBLOCK(a, p, ptr)	((uintptr_t)((char*)ptr - (char)(p+1))/(a)->l.block_size)

#define BA_DIVIDE(a, b)	    ((a) / (b) + (!!((a) & ((b)-1))))
#define BA_PAGESIZE(a)	    (sizeof(struct ba_page)			\
			     + (a)->l.offset + (a)->l.block_size) 
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

    PAGE_LOOP(a, {
	uint32_t blocks_used;
	blocks_used = p->used;
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

/* #define BA_ALIGNMENT	8 */

#ifndef BA_CMEMSET
static INLINE void cmemset(char * dst, char * src, size_t s,
			   size_t n) {
    memcpy(dst, src, s);

    for (--n,n *= s; n >= s; n -= s,s <<= 1)
	memcpy(dst + s, dst, s);

    if (n) memcpy(dst + s, dst, n);
}
#define BA_CMEMSET(dst, src, s, n)	cmemset(dst, src, s, n)
#endif

EXPORT void ba_init(struct block_allocator * a, uint32_t block_size,
		    uint32_t blocks) {
    uint32_t page_size;

    if (!block_size) BA_ERROR("ba_init called with zero block_size\n");

    if (block_size < sizeof(struct ba_block_header)) {
	block_size = sizeof(struct ba_block_header);
    }

#ifdef BA_ALIGNMENT
    if (block_size & (BA_ALIGNMENT - 1))
	block_size += (BA_ALIGNMENT - (block_size & (BA_ALIGNMENT - 1)));
#endif

    if (!blocks) blocks = 512;

    ba_init_layout(&a->l, block_size, blocks);
    ba_align_layout(&a->l);

    a->empty = a->first = NULL;
    a->last_free = NULL;

#ifdef BA_DEBUG
    fprintf(stderr, "blocks: %u block_size: %u page_size: %u\n",
	    blocks, block_size, page_size);
#endif


#ifndef ctz32
# define ctz32 __builtin_ctz
#endif
    a->magnitude = ctz32(round_up32(BA_PAGESIZE(a))); 
    a->num_pages = 0;
    a->empty_pages = 0;
    a->max_empty_pages = BA_MAX_EMPTY;

    /* we start with management structures for BA_ALLOC_INITIAL pages */
    a->allocated = BA_ALLOC_INITIAL;
    a->pages = (ba_p*)malloc(BA_ALLOC_INITIAL * sizeof(ba_p));
    if (!a->pages) ba_error("nomem");
    memset(a->pages, 0, BA_ALLOC_INITIAL * sizeof(ba_p));

#ifdef BA_DEBUG
    fprintf(stderr, " -> blocks: %u block_size: %u page_size: %u mallocing size: %lu, next pages: %u magnitude: %u\n",
	    a->l.blocks, block_size, BA_PAGESIZE(a)-sizeof(struct ba_page),
	    BA_PAGESIZE(a),
	    round_up32(BA_PAGESIZE(a)),
	    a->magnitude);
#endif
}

static INLINE void ba_free_page(struct block_allocator * a, ba_p p) {
    p->first = BA_BLOCKN(a, p, 0);
    p->used = 0;

    if (a->blueprint)
	BA_CMEMSET((char*)(p+1), (char*)a->blueprint, a->l.block_size, a->l.blocks);

#ifdef BA_CHAIN_PAGE
    do {
	char * ptr = (char*)(p+1);
	
	while (ptr < BA_LASTBLOCK(a, p)) {
#ifdef BA_DEBUG
	    MEM_RW(((ba_b)ptr)->magic);
	    ((ba_b)ptr)->magic = BA_MARK_FREE;
#endif
	    ((ba_b)ptr)->next = (ba_b)(ptr+a->l.block_size);
	    ptr+=a->l.block_size;
	}
	BA_LASTBLOCK(a, p)->next = NULL;
    } while (0);
#else
    p->first->next = BA_ONE;
#endif
}

EXPORT INLINE void ba_free_all(struct block_allocator * a) {
    if (!a->allocated) return;

    PAGE_LOOP(a, {
	MEM_RW_RANGE(p, BA_PAGESIZE(a));
	free(p);
    });

    a->free_blk = NULL;
    a->alloc = NULL;
    a->num_pages = 0;
    a->empty_pages = 0;
    a->empty = a->first = NULL;
    a->last_free = NULL;
    a->allocated = BA_ALLOC_INITIAL;
    memset(a->pages, 0, a->allocated * sizeof(ba_p));
}

EXPORT void ba_count_all(struct block_allocator * a, size_t *num, size_t *size) {
    ba_b t;
    size_t n = 0;

    /* fprintf(stderr, "page_size: %u, pages: %u\n", BA_PAGESIZE(a), a->num_pages); */
    *size = BA_BYTES(a) + a->num_pages * BA_PAGESIZE(a);
    PAGE_LOOP(a, {
	if (p == a->alloc) continue;
	n += p->used;
    });

    t = a->free_blk;
    if (t) {
	n += a->l.blocks;
	while (t) {
	    n--;
	    if (t->next == BA_ONE) {
		t = (ba_b)(((char*)t) + a->l.block_size);
		t->next = (ba_b)(size_t)!(t == BA_LASTBLOCK(a, a->alloc));
	    } else {
		t = t->next;
	    }
	}
    }

    *num = n;
}

EXPORT void ba_destroy(struct block_allocator * a) {
    PAGE_LOOP(a, {
	MEM_RW_RANGE(p, BA_PAGESIZE(a));
	free(p);
    });

    a->free_blk = NULL;
    a->alloc = NULL;
    MEM_RW_RANGE(a->pages, BA_BYTES(a));
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
    a->pages = (ba_p*)realloc(a->pages, a->allocated * sizeof(ba_p));
    if (!a->pages) ba_error("nomem");
    memset(a->pages+old, 0, old * sizeof(ba_p));
    for (n = 0; n < old; n++) {
	ba_p * b = a->pages + n;

	while (*b) {
	    uint32_t h = hash1(a, BA_LASTBLOCK(a, *b)) & BA_HASH_MASK(a);
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

    a->pages = (ba_p*)realloc(a->pages, a->allocated * sizeof(ba_p));
    if (!a->pages) ba_error("nomem");
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
	    void * ptr = BA_LASTBLOCK(a, p);
	    hval = hash1(a, ptr);
	    fprintf(stderr, "%u\t%X[%u]\t%p-%p\t%X\n", i, hval, hval & BA_HASH_MASK(a), p+1, ptr, (unsigned int)((uintptr_t)ptr >> a->magnitude));
	    p = p->hchain;
	}
    }
}
#endif

/*
 * insert the pointer to an allocated page into the
 * hashtable. uses linear probing and open allocation.
 */
static INLINE void ba_htable_insert(const struct block_allocator * a,
				    ba_p p) {
    uint32_t hval = hash1(a, BA_LASTBLOCK(a, p));
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
    uint32_t hval = hash1(a, BA_LASTBLOCK(a, p));
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
	if (BA_CHECK_PTR(a, p, ptr)) {
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

    if (a->empty_pages > a->num_pages) {
	fprintf(stderr, "too many empty pages.\n");
	bad = 1;
    }

    for (n = 0; n < a->allocated; n++) {
	uint32_t hval;
	int found = 0;
	p = a->pages[n];

	while (p) {
	    if (!p->first && p != a->first) {
		if (p->prev || p->next) {
		    fprintf(stderr, "page %p is full but in list. next: %p"
			     "prev: %p\n",
			    p, p->next,
			    p->prev);
		    bad = 1;
		}
	    }

	    hval = hash1(a, BA_LASTBLOCK(a, p)) & BA_HASH_MASK(a);

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

static INLINE void ba_alloc_page(struct block_allocator * a) {
    ba_p p;

    if (unlikely(a->num_pages == a->allocated)) {
	ba_grow(a);
    }

    a->num_pages++;
    p = (ba_p)malloc(BA_PAGESIZE(a));
    if (!p) BA_ERROR("no mem. alloc returned zero.");
    ba_free_page(a, p);
    p->next = p->prev = NULL;
    a->alloc = p;
    ba_htable_insert(a, p);
#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_alloc after insert", __FILE__, __LINE__);
#endif
#ifdef BA_DEBUG
    MEM_RW(BA_LASTBLOCK(a, p)->magic);
    BA_LASTBLOCK(a, p)->magic = BA_MARK_FREE;
    MEM_RW(BA_BLOCKN(a, p, 0)->magic);
    BA_BLOCKN(a, p, 0)->magic = BA_MARK_ALLOC;
    ba_check_allocator(a, "ba_alloc after insert.", __FILE__, __LINE__);
#endif
}

EXPORT void ba_low_alloc(struct block_allocator * a) {
    /* fprintf(stderr, "ba_alloc(%p)\n", a); */
#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_alloc top", __FILE__, __LINE__);
#endif
    if (a->alloc) {
	a->alloc->first = NULL;
	a->alloc->used = a->l.blocks;
    }

    if (a->first) {
	a->alloc = a->first;
	DOUBLE_SHIFT(a->first);
    } else if (a->empty) {
	a->alloc = a->empty;
	SINGLE_SHIFT(a->empty);
	a->empty_pages--;
    } else {
	a->alloc = NULL;
	ba_alloc_page(a);
    }
    a->alloc->next = NULL;

    a->free_blk = a->alloc->first;

#ifdef BA_DEBUG
    if (!a->free_blk) {
	ba_show_pages(a);
	BA_ERROR("a->first has no first block!\n");
    }
#endif

#ifdef BA_DEBUG
    ba_check_allocator(a, "ba_alloc after grow", __FILE__, __LINE__);
#endif
}

EXPORT void ba_low_free(struct block_allocator * a, ba_p p, ba_b ptr) {
    if (!p->used) {
	INC(free_empty);
	DOUBLE_UNLINK(a->first, p);
#ifndef BA_CHAIN_PAGE
	/* reset the internal list. this avoids fragmentation which would otherwise
	 * happen when reusing pages. Since that is cheap here, we do it.
	 */
	p->first = BA_BLOCKN(a, p, 0);
	p->first->next = BA_ONE;
#endif
	SINGLE_LINK(a->empty, p);
	a->empty_pages ++;
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
	if (BA_CHECK_PTR(a, p, ptr)) {
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
	    n, p, p+1, BA_LASTBLOCK(a, p), hash1(a, BA_LASTBLOCK(a,p)),
	    hash1(a, BA_LASTBLOCK(a,p)) & BA_HASH_MASK(a),
	    a->num_pages);
#endif

    ba_htable_delete(a, p);
    MEM_RW_RANGE(*p, BA_PAGESIZE(a));

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

#ifdef __cplusplus
}
#endif

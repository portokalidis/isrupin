#ifndef FILTER_HPP

#include "log.hpp"
#include "debug.h"
#include "libcrossdev.hpp"
#include "compiler.h"

#define FILTER_BITMAP	0
#define FILTER_WLOG	1

#define FILTER_TYPE FILTER_WLOG

//! 128M 4-byte buckets covers all 4G space
#define BITMAP_LEN 		(128 * 1024 * 1024) 
#define BITMAP_ADDRESSABLE	(96 * 1024 * 1024)
#define BITMAP_BUCKET_LEN	4
#define BITMAP_BUCKET_SHIFT	2
#define BITMAP_BUCKET_MASK	0x3

#define WLOGFILTER_PAGE_SIZE	4096
#define WLOGFILTER_IDXMASK	0xfffff //!< 1048576 Entries
#define WLOGFILTER_SLOTS 	(WLOGFILTER_IDXMASK + 1)
#define WLOGFILTER_LEN		(WLOGFILTER_SLOTS * sizeof(wlogfilter_entry))
#define WLOGFILTER_FN_PREFIX	"ReassureCheckpoint"
#define WLOGFILTER_ENTRY(fl)	((struct wlogfilter_entry *)(fl)->cbuf + \
		((fl)->idx & WLOGFILTER_IDXMASK))
#define WLOGFILTER_DATABUF_SIZE	8192

typedef UINT32 cbuf_idx_t;


/********************************/
/*       BITMAP FILTER          */
/********************************/
#if FILTER_TYPE == FILTER_BITMAP

struct filter_struct {
	unsigned char bitmap[BITMAP_LEN];
};


/********************************/
/*     WRITES LOG FILTER        */
/********************************/
#elif FILTER_TYPE == FILTER_WLOG
//#include "shared_cbuf.h"

/**
 * The writes log entry structure stores a written address and the length of the
 * write. It should not break page alignment (e.g., an entry cannot live between
 * two pages, if we started writing an array from the beginning of a page).
 */
struct wlogfilter_entry {
	ADDRINT addr;		//!< Address written
	UINT32 len;		//!< Length of write
} LINUX_PACKED;


/** 
 * Writes log based filter structure.
 */
struct filter_struct {
	cache_entry_t *cache; //! Associative cache

	cbuf_idx_t idx; 	//!< Cursor index
	unsigned char *cbuf; 	//!< Pointer to beginning of buffer 
	//! The Guard pointer marks the end of the buffer 
	unsigned char *guard; 	
	//size_t cbuf_elemno; //!< Number of elements in cbuf

	//! A shared ring buffer to transfer original data quickly
	//struct shared_cbuf databuf; 

	//! File descriptor to write addresses when cbuf is full
	fstream *fio; 

	//! Filename for temporary file used to store the filter
	char tmpfn[TEMPFILE_MAXPATH];
};
#endif



//! Filter type
typedef struct filter_struct filter_t;


void filter_child_rollback(filter_t *fp, int pipe);
bool filter_parent_rollback(int pipe);
void filter_init(filter_t *fl);
void filter_cleanup(filter_t *fl);
bool filter_handle_internal_fault(filter_t *fl, 
		const EXCEPTION_INFO *pExceptInfo);



/********************************/
/*       BITMAP FILTER          */
/********************************/
#if FILTER_TYPE == FILTER_BITMAP

static inline void FLogMarkB(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = (addr >> BITMAP_BUCKET_SHIFT); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
}

static inline ADDRINT FLogMarkW(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = (addr >> BITMAP_BUCKET_SHIFT); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
	return ((addr & 0x3) == 0x3);
}

static inline void FLogMarkExtW(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = ((addr >> BITMAP_BUCKET_SHIFT) + 1); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
}

static inline ADDRINT FLogMarkL(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = (addr >> BITMAP_BUCKET_SHIFT); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
	return (addr & BITMAP_BUCKET_MASK);
}

static inline void FLogMarkExtL(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = ((addr >> BITMAP_BUCKET_SHIFT) + 1); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
}

static inline ADDRINT FLogMarkQ(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = (addr >> BITMAP_BUCKET_SHIFT); // 4-byte buckets
	// We assert two bits at once
	*(UINT16 *)(filter->bitmap + (bit >> 3)) |= (UINT16)0x3 << (bit & 0x7);
	return (addr & BITMAP_BUCKET_MASK);
}

static inline void FLogMarkExtQ(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = ((addr >> BITMAP_BUCKET_SHIFT) + 2); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
}

static inline ADDRINT FLogMarkDQ(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = (addr >> BITMAP_BUCKET_SHIFT); // 4-byte buckets
	// We assert four bits at once
	*(UINT16 *)(filter->bitmap + (bit >> 3)) |= (UINT16)0xf << (bit & 0x7);
	return (addr & BITMAP_BUCKET_MASK);
}

static inline void FLogMarkExtDQ(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = ((addr >> BITMAP_BUCKET_SHIFT) + 4); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
}

static inline ADDRINT FLogMarkQQ(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = (addr >> BITMAP_BUCKET_SHIFT); // 4-byte buckets
	// We assert eight bits at once
	*(UINT16 *)(filter->bitmap + (bit >> 3)) |= (UINT16)0xff << (bit & 0x7);
	return (addr & BITMAP_BUCKET_MASK);
}

static inline void FLogMarkExtQQ(filter_t *filter, ADDRINT addr)
{
	ADDRINT bit = ((addr >> BITMAP_BUCKET_SHIFT) + 8); // 4-byte buckets
	filter->bitmap[bit >> 3] |= 0x1 << (bit & 0x7); // bitmap assertion
}

/********************************/
/*     WRITES LOG FILTER        */
/********************************/
#elif FILTER_TYPE == FILTER_WLOG

/**
 * Macro to mark address range in the filter.
 * The current index is increased after accessing the entry to ensure a valid
 * state even when writing in the guard page
 */
#define FLOGMARK(fl, address, length) \
	do {\
		struct wlogfilter_entry *entry;\
		entry = WLOGFILTER_ENTRY(fl);\
		entry->addr = address;\
		entry->len = length;\
		(fl)->idx++;\
	} while (0)
#endif

#endif

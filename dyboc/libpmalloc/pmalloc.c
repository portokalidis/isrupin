#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <errno.h>

//#include <glib.h> //For datastructures
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>

#include <assert.h>
#include <dlfcn.h>

#include "pmalloc.h"


/* Pointer to original posix_memalign() function */
static int (*orig_posix_memalign)(void **, size_t, size_t);

/* Pointer to original free() function */
static void (*orig_free)(void *);



// Special error page for generating faults
static void *error_page = NULL;
// Fault generation function to use
static void (*generate_fault)(void);




/**
 * Set information about pbuffer in the left guard page.
 * Generic image of a pbuffer in memory:
 * |left left pad|left guard|left pad|buf|right pad|right guard|
 *
 * @param leftguard Pointer to left guard page
 * @param ptr_off Offset from left guard page to beginning of pbuffer
 * @param buflen Buffer length
 * @param rplen Right pad length
 */
static void set_pbuffer_info(void *leftguard, size_t ptr_off,
		size_t buflen, size_t rplen)
{
	*(size_t *)(leftguard) = ptr_off;
	*(size_t *)(leftguard + sizeof(size_t)) = buflen;
	*(size_t *)(leftguard + 2 * sizeof(size_t)) = rplen;
}

/**
 * Get information about pbuffer in the left guard page.
 * Generic image of a pbuffer in memory:
 * |left left pad|left guard|left pad|buf|right pad|right guard|
 *
 * @param leftguard Pointer to left guard page
 * @param ptr_off Offset from left guard page to beginning of pbuffer
 * @param buflen Buffer length
 * @param rplen Right page length
 */
static void get_pbuffer_info(void *leftguard, size_t *ptr_off,
		size_t *buflen, size_t *rplen)
{
	*ptr_off = *(size_t *)leftguard;
	*buflen = *(size_t *)(leftguard + sizeof(size_t));
	*rplen = *(size_t *)(leftguard + 2 * sizeof(size_t));
}

/* Store the magic number, as the means to
 * differentiate dyboc related segfaults.
 */
static void mark_magic_number(void *ptr, size_t len)
{
	void *start, *end;
	uint32_t magic;

	for (start = ptr, end = ptr + len; (start + 3) < end; start += 4) {
		*(uint32_t *)start = PMALLOC_MAGIC_NUMBER;
	}
	for (magic = PMALLOC_MAGIC_NUMBER; start < end; start++, magic >>= 8) {
		*(uint8_t *)start = (uint8_t)magic;
	}

}

static void check_magic_number(void *ptr, size_t len)
{
	void *start, *end;
	uint32_t magic;

	/* Check that the magic values (canaries) are there */
	for (start = ptr, end = ptr + len; (start + 3) < end; start += 4) {
		if (*(uint32_t *)start != PMALLOC_MAGIC_NUMBER) {
#ifdef PMALLOC_DEBUG
			fprintf(stderr, "unprotect_area: magic number does "
					"not match\n");
#endif
			generate_fault();
		}
	}
	for (magic = PMALLOC_MAGIC_NUMBER; start < end; start++, magic >>= 8) {
		if (*(uint8_t *)start != (uint8_t)magic) {
#ifdef PMALLOC_DEBUG
			fprintf(stderr, "unprotect_area: magic number "
					"does not match\n");
#endif
			generate_fault();
		}
	}


}

static void raise_signal()
{
	raise(SIGSEGV);
}

static inline void access_error_page()
{
	char *p = (char *)error_page;

	/* This should throw a segfault */
	*p = 'A';
}


#if 0
/* 
 * pmalloc
 *
 */
void *pmalloc(size_t len)
{
	void *buf, *ptr;
	size_t real_len, offset;

#ifdef PMALLOC_WHITELIST
	if (wlist) {
		size_t *wl;
		for (wl = wlist; *wl > 0; wl++)
			if (*wl == len) {
				len++;
				//fprintf(stderr, "Increase size of allocation %d\n", len);
				break;
			}
	}
	/*
	if (len == 36865) {
		len++;
	}
	*/
#endif

#ifdef PMALLOC_DEFAULT
	/* We need to allocate 2 pages + all the pages necessary to store len
	 * bytes */
	real_len = len + 2 * PAGE_SIZE;

	/* Offset of buffer in page 1 */
	offset = PAGE_SIZE - (len & ~PAGE_MASK);
#elif defined(PMALLOC_UNDER)
	/* We need to allocate 1 page + all the pages necessary to store len
	 * bytes */
	real_len = len + PAGE_SIZE;

	/* Offset of buffer in page 1 */
	offset = 0;
#elif defined(PMALLOC_OVER)
	/* We need to allocate all the pages necessary to store len+size_t
	 * bytes + 1 page */
	real_len = len + sizeof(size_t) + PAGE_SIZE;

	/* Offset of buffer in page 1 */
	offset = PAGE_SIZE - ((len + sizeof(size_t)) & ~PAGE_MASK);
#else
# error "Unspecified pmalloc behavior"
#endif

	// Align to page size
	if (real_len & ~PAGE_MASK)
		real_len = (real_len & PAGE_MASK) + PAGE_SIZE;
	// If page was aligned, offset = 0
	if (offset == PAGE_SIZE)
		offset = 0;

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmalloc: len=%lu, real_len=%lu\n", len, real_len);
#endif

	if((buf = mmap(NULL, real_len, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == (void *)-1) {
		perror("pmalloc/mmap");
		exit(1);
	}

#ifdef PMALLOC_DEFAULT
	// Mark pages as magic
	// Mark first pages + unused offset in 2nd page
	mark_magic_number(buf, PAGE_SIZE + offset); 
	// Mark last page
	mark_magic_number(buf + PAGE_SIZE + len + offset, PAGE_SIZE); 

	/* We store the length twice, because we need it in the beginning of the
	 * segment, and we can keep it in the protected region as well to double
	 * check for heap corruption (ie: the user wrote to addresses BEFORE his
	 * memory)
	*/
	*(size_t *)buf = len;
	*(size_t *)(buf + PAGE_SIZE + offset + len) = len;

	// Protect first page
	if(mprotect(buf, PAGE_SIZE, PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	// Protect last page
	if(mprotect(buf + PAGE_SIZE + offset + len, PAGE_SIZE, 
				PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	ptr = buf + PAGE_SIZE + offset;
#elif defined(PMALLOC_UNDER)
	// Mark page guard page with magic number
	mark_magic_number(buf, PAGE_SIZE);

	/* Store the length of the allocated area in the guard page */
	*(size_t *)(buf) = len;

	// Protect first page
	if(mprotect(buf, PAGE_SIZE, PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	ptr = buf + PAGE_SIZE;
#elif defined(PMALLOC_OVER)
	// Mark guard page with magic number
	mark_magic_number(buf + len + offset + sizeof(size_t), PAGE_SIZE);

	/* We store the length twice, because we need it in the beginning of the
	* segment, and we can keep it in the protected region as well to double 
	* check for heap corruption (ie: the user wrote to addresses BEFORE his
	* memory)
	*/
	*(size_t *)(buf + offset) = len;
	*(size_t *)(buf + offset + sizeof(size_t) + len) = len;

	// Protect last page
	if(mprotect(buf + len + offset + sizeof(size_t), 
				PAGE_SIZE, PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	ptr = buf + offset + sizeof(size_t);
#else
# error "Unspecified pmalloc behavior"
#endif

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "         start=%8p end(excl)=%8p ptr=%8p\n", buf, 
			buf + real_len, ptr);
#endif
#if 0
	if (buf == (void *)0xb60ab000) 
		asm ("int3\n\t");
#endif
	return ptr;
}
#endif

/*
 * Unprotect an area returned by malloc, and return the begin of the mapped area
 * and its real length
 */
static void *unprotect_area(void *ptr, size_t *real_len, size_t *lenp)
{
	void *start;
	size_t offset, len;

#ifdef PMALLOC_DEFAULT
	// Calculate the beginning of the area we allocated
	start = (void *)((long)ptr & PAGE_MASK) - PAGE_SIZE;

	/* Unprotect 1st guard page */
	if (mprotect(start, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		generate_fault();
	}

	// Get the length of the area
	len = *(size_t *)start;
	/* Offset of ptrfer in page 1 */
	offset = (len & ~PAGE_MASK);
	if (offset != 0) // len not aligned
		offset = PAGE_SIZE - offset;

	*real_len = 2 * PAGE_SIZE + len + offset;
	*lenp = len;

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "unprotect_area: start=%p, ptr=%p, len=%lu, "
			"real_len=%lu\n", start, ptr, len, *real_len);
# endif

	/* Unprotect last guard page */
	if (mprotect(ptr + len, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		generate_fault();
	}

	/* Check to make sure lengths are consistant. If not, advise user to 
	 * try using the (not yet written) underflow version of this library */
	if(*(size_t *)(ptr + len) != len) {
		fprintf(stderr, "Error: heap corruption. Inconsistant "
				"allocation sizes.\n");
		generate_fault();
	}

	check_magic_number(start + sizeof(size_t),
			PAGE_SIZE + offset - sizeof(size_t));

#elif defined(PMALLOC_UNDER)
	// Calculate the beginning of the area we allocated
	start = ptr - PAGE_SIZE;

	/* Unprotect guard page */
	if (mprotect(start, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		generate_fault();
	}

	// Get the length of the area
	len = *(size_t *)start;
	/* Offset of end of ptrfer */
	offset = (len & ~PAGE_MASK);
	if (offset != 0) // len not aligned
		offset = PAGE_SIZE - offset;

	*real_len = len + offset + PAGE_SIZE;
	*lenp = len;

	check_magic_number(start + sizeof(size_t), 
			PAGE_SIZE - sizeof(size_t));

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "unprotect_area: start=%p, ptr=%p, len=%lu, "
			"real_len=%lu\n", start, ptr, len, *real_len);
# endif

#elif defined(PMALLOC_OVER)
	// Get the length of the allocated area
	len = *(size_t *)(ptr - sizeof(size_t));

	// Calculate the beginning of the area we allocated
	start = (void *)((long)(ptr - sizeof(size_t)) & PAGE_MASK);

	// Offset of ptr from beginning of allocated area
	offset = ptr - start;

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "unprotect_area: start=%p, ptr=%p, len=%lu\n", 
			start, ptr, len);
# endif

	/* Unprotect guard page */
	if (mprotect(ptr + len, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		generate_fault();
	}

	*real_len = len + offset + PAGE_SIZE;
	*lenp = len;

	check_magic_number(start + len + offset + sizeof(size_t), 
			PAGE_SIZE - sizeof(size_t));

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "start=%p, ptr=%p, len=%lu, real_len=%lu\n", 
			start, ptr, len, *real_len);
# endif

	// Check that the two locations were we stored the length of the ptrfer
	// agree
	if (len != *(size_t *)(ptr + len)) {
		fprintf(stderr, "Error: heap corruption. Inconsistant "
				"allocation sizes.\n");
		generate_fault();
	}
#else
# error "Unspecified unprotect_area behavior"
#endif

	return start;
}

/*
 * prealloc
 */
void *prealloc(void *ptr, size_t size)
{
	void *new_ptr, *old_area;
	size_t old_len, old_arealen;

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "prealloc: old_ptr=%p new size=%lu\n", ptr, size);
#endif

	if (!ptr)
		return pmalloc(size);
	else if (size == 0) {
		pfree(ptr);
		return NULL;
	}

	/* XXX: Dummy version.
	 * Allocate new area, find information about older area,
	 * copy data, free old area.
	 */
	new_ptr = pmalloc(size);
	old_area = unprotect_area(ptr, &old_arealen, &old_len);
	if (old_len < size)
		size = old_len;
	memcpy(new_ptr, ptr, size);
	if (munmap(old_area, old_arealen) == -1) {
		perror("prealloc/munmap");
		generate_fault();
	}
	return new_ptr;
}

void *pcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	ptr = pmalloc(nmemb * size);
	if (ptr)
		memset(ptr, 0, nmemb * size);
	return ptr;
}


/**
 * Free a pbuffer.
 * Unprotect guards and call the original free with main pointer.
 * leftguard is always at the page preceding the buffer and contains the
 * information saved with pbuffer_set_info().
 *
 * Generic image of a pbuffer in memory:
 * |left left pad|left guard|left pad|buf|right pad|right guard|
 *
 * @param memptr Pointer to user buffer
 */
void pfree(void *memptr)
{
	void *leftguard, *rightguard, *rightpad, *leftpad;
	size_t ptr_off, buflen, rplen, lplen;

	/* Left pad may not exist */
	leftpad = (void *)((unsigned long)memptr & PAGE_MASK);

	/* Preceding page is the left guard*/
	leftguard = leftpad - PAGE_SIZE;

	/* Make left guard RW */
	if (mprotect(leftguard, PAGE_SIZE, PROT_READ|PROT_WRITE) != 0)
		generate_fault();

	get_pbuffer_info(leftguard, &ptr_off, &buflen, &rplen);

	/* left pad length */
	lplen = memptr - leftpad;
	if (lplen)
		check_magic_number(leftpad, lplen);

	/* Right pad */
	rightpad = memptr + buflen;

	/* Right guard */
	rightguard = rightpad + rplen;

	if (rplen)
		check_magic_number(rightpad, rplen);

	/* Make right guard RW */
	if (mprotect(rightguard, PAGE_SIZE, PROT_READ|PROT_WRITE) != 0)
		generate_fault();

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pfree: %p -->  "
			"|%lu|%lu|%lu|*%lu*|%lu|%lu|\n",
			memptr, ptr_off, 
			PAGE_SIZE, lplen, buflen, rplen, PAGE_SIZE);
#endif

	orig_free(leftguard - ptr_off);
}


/**
 * Protected malloc.
 * Goal is protect overflow first
 * |left_guard|leftpad|buffer|right_guard|
 *
 * @param size Buffer size to allocate
 * @return Pointer to allocated user buffer
 */
void *pmalloc(size_t size)
{
	size_t newsize, lplen;
	void *ptr;

	/* Calculate if we need a leftpad */
	lplen = size & ~PAGE_MASK;
	if (lplen)
		lplen = PAGE_SIZE - lplen;

	newsize = size + lplen + 2 * PAGE_SIZE;

	if (orig_posix_memalign(&ptr, PAGE_SIZE, newsize) != 0)
		return NULL;

	/* Leftpad length */
	if (lplen)
		/* Insert magic number in the left pad */
		mark_magic_number(ptr + PAGE_SIZE, lplen);

	set_pbuffer_info(ptr, 0, size, 0);

	if (mprotect(ptr, PAGE_SIZE, PROT_NONE) != 0) {
		perror("pmalloc/mprotect");
		goto release_mem;
	}

	if (mprotect(ptr + newsize - PAGE_SIZE, PAGE_SIZE, PROT_NONE) != 0) {
		perror("pmalloc/mprotect");
		goto revert_lguard;
	}

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmalloc: %lu -->  "
			"|0|%lu|%lu|*%lu*|0|%lu|\n",
			size, 
			PAGE_SIZE, lplen, size, PAGE_SIZE);
#endif

	return (ptr + PAGE_SIZE + lplen);

revert_lguard:
	mprotect(ptr, PAGE_SIZE, PROT_NONE);
release_mem:
	orig_free(ptr);
	return NULL;
}


/**
 * Guarded posix_memalign()
 * Goal is
 * |leftpad|left_guard|aligned_buffer|rightpad|right_guard|
 *
 * @param memptr Allocated aligned buffer
 * @param alignment Requested alignment
 * @param size Size of requested buffer
 */
int pmemalign(void **memptr, size_t alignment, size_t size)
{
	int ret;
	void *ptr;
	size_t newsize, newalignment, leftpad, rightpad;

	/* Check that alignment is a power of two (Bit Twiddling hacks)
	 * Check that alignment is a multiple of sizeof(void *) */
	if (!(alignment && !(alignment & (alignment - 1)))
			|| (alignment & (sizeof(void *) - 1))) {
		/* posix_memalign will fail */
		ret = orig_posix_memalign(memptr, alignment, size);
		assert(ret != 0);
		return ret;
	}


	/* Force (at least) page alignment to accommodate the guard */
	newalignment = (alignment < PAGE_SIZE)? PAGE_SIZE : alignment;

	/* Right pad */
	rightpad = PAGE_SIZE - (size & ~PAGE_MASK);

	/* What we are going to ask for */
	newsize = newalignment /* leftpad + leftguard */
		+ size + rightpad /* size + rightpad */
		+ PAGE_SIZE; /* rightguard */

	/* Left pad */
	leftpad = newalignment - PAGE_SIZE;

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "posix_memalign: %lu %lu -->  "
			"|%lu|%lu|0|*%lu*|%lu|%lu|\n",
			alignment, size, leftpad, PAGE_SIZE, size,
			rightpad, PAGE_SIZE);
#endif

	if ((ret = orig_posix_memalign(&ptr, newalignment, newsize)) != 0)
		return ret; /* Memory allocation failed */

	/*
	fprintf(stderr, "ptr=%p, left guard=%p, right guard %p\n",
			ptr, ptr + leftpad, 
			ptr + newalignment + size + rightpad);
	*/

	set_pbuffer_info(ptr + leftpad, leftpad, size, rightpad);

#if 0
	/* Store the offset of the real pointer (==leftpad) from the
	 * left guard */
	*(size_t *)(ptr + leftpad) = leftpad;
	/* Store the offset of the rightpad from the buffer pointer */
	*(size_t *)(ptr + leftpad + sizeof(size_t)) = size;
	/* Store the offset of the right guard page from the buffer pointer */
	*(size_t *)(ptr + leftpad + 2 * sizeof(size_t)) = size + rightpad;
#endif

	/* Protect left guard page */
	if (mprotect(ptr + leftpad, PAGE_SIZE, PROT_NONE) != 0) {
		perror("posix_memalign/mprotect");
		goto release_mem;
	}

	/* Insert magic number in the right pad */
	mark_magic_number(ptr + newalignment + size, rightpad);

	/* Protect right guard page */
	if (mprotect(ptr + newalignment + size + rightpad, 
				PAGE_SIZE, PROT_NONE) != 0) {
		perror("posix_memalign/mprotect");
		goto release_leftguard;
	}

	*memptr = ptr + leftpad + PAGE_SIZE;
	return ret;

release_leftguard:
	mprotect(ptr + leftpad, PAGE_SIZE, PROT_READ | PROT_WRITE);
release_mem:
	orig_free(ptr);
	return EINVAL;
}





__attribute__((constructor)) 
void __init(void)
{

	error_page = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (error_page == MAP_FAILED) {
map_failed:
		generate_fault = raise_signal;
		perror("init:mmap");
	} else {
		generate_fault = access_error_page;
		mark_magic_number(error_page, PAGE_SIZE);
		if (mprotect(error_page, PAGE_SIZE, PROT_NONE) != 0)
			goto map_failed;
	}
#ifdef PMALLOC_WHITELIST
	whitelist_init();
#endif

	if (!orig_posix_memalign) {
		orig_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
		assert(orig_posix_memalign != NULL);
	}

	if (!orig_free) {
		orig_free = dlsym(RTLD_NEXT, "free");
		assert(orig_free != NULL);
	}

}

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>

#include <assert.h>
#include <dlfcn.h>

#include "pmalloc.h"


/* Segments allocated with pmalloc will return a user buffer with this 
 * alignment */
#define PMALLOC_ALIGNMENT (sizeof(void *))


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

#if 0
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
#endif

/**
 * Protected realloc.
 *
 * @param memptr Pointer to previous buffer
 * @param size New required size
 * @return Pointer to reallocated user buffer
 */
void *prealloc(void *memptr, size_t size)
{
	void *leftguard, *rightguard, *rightpad, *leftpad, *newptr;
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

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "prealloc: %p %lu -->  "
			"|%lu|%lu|%lu|*%lu*|%lu|%lu|\n",
			memptr, size, 
			ptr_off, PAGE_SIZE, lplen, buflen, rplen, PAGE_SIZE);
#endif

	if (buflen == size) { /* same size */
		/* Re-protect guard page */
		if (mprotect(leftguard, PAGE_SIZE, PROT_NONE) != 0)
			generate_fault();
		return memptr;
	}
#if 0
	else if (buflen > size) { /* reduction in size */
		lplen += buflen - size;
		set_pbuffer_info(leftguard, ptr_off, size, rplen);
		memmove(leftguard + PAGE_SIZE + lplen, memptr, size);
		memptr += buflen - size;
		mark_magic_number(leftpad, lplen);
		/* Re-protect guard page */
		if (mprotect(leftguard, PAGE_SIZE, PROT_NONE) != 0)
			generate_fault();
#ifdef PMALLOC_DEBUG
		fprintf(stderr, "prealloc shorten: %p %lu -->  "
				"|%lu|%lu|%lu|*%lu*|%lu|%lu|\n",
				memptr, size, 
				ptr_off, PAGE_SIZE, lplen, buflen, 
				rplen, PAGE_SIZE);
#endif
		return memptr;
	} 
#endif
#if 0
	else if ((rplen + lplen + buflen) >= size) {
	/* We are lucky to have enough space in the padding for the
	 * reallocation.
	 * NOTE: We are leaking the magic number here, but it is
	 * not so important since the guard pages are still there. An attacker
	 * can only delay detection not bypass the guards. */

		/* Extending to the right does not require any data moving. */
		size_t l = buflen;

		if ((buflen + rplen) > size) {
			l = size;
			rplen -= (size - buflen);
			mark_magic_number(memptr + l, rplen);
		} else {
			l += rplen;
			rplen = 0;
		}

		if (l < size) {
			/* We need to also use the leftpad. Some moving
			 * required. */
			if ((l + lplen) > size) {
				mark_magic_number(leftpad, lplen);
			} else {
				lplen = 0;
			}
			l = size;

			memmove(leftguard + PAGE_SIZE + lplen, memptr, buflen);
		}

		set_pbuffer_info(leftguard, ptr_off, l, rplen);

		/* Re-protect guard page */
		if (mprotect(leftguard, PAGE_SIZE, PROT_READ|PROT_WRITE) != 0)
			generate_fault();


#ifdef PMALLOC_DEBUG
		fprintf(stderr, "prealloc fast: %p %lu -->  "
				"|%lu|%lu|%lu|*%lu*|%lu|%lu|\n",
				memptr, size, 
				ptr_off, PAGE_SIZE, lplen, l, 
				rplen, PAGE_SIZE);
#endif

		return (leftguard + PAGE_SIZE + lplen);
	}
#endif

	/* Make right guard RW */
	if (mprotect(rightguard, PAGE_SIZE, PROT_READ|PROT_WRITE) != 0)
		generate_fault();

	newptr = pmalloc(size);
	if (!newptr)
		return newptr;
	memcpy(newptr, memptr, MIN(buflen, size));
	munmap(leftguard - ptr_off, ptr_off + buflen + lplen + 
			rplen + 2 * PAGE_SIZE);
	return newptr;
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
 * @param ptr Pointer to user buffer
 */
void pfree(void *ptr)
{
	void *leftguard, *rightpad, *leftpad;
	size_t ptr_off, buflen, rplen, lplen;

	/* Left pad may not exist */
	leftpad = (void *)((unsigned long)ptr & PAGE_MASK);

	/* Preceding page is the left guard*/
	leftguard = leftpad - PAGE_SIZE;

	/* Make left guard RW */
	if (mprotect(leftguard, PAGE_SIZE, PROT_READ|PROT_WRITE) != 0)
		generate_fault();

	get_pbuffer_info(leftguard, &ptr_off, &buflen, &rplen);

	/* left pad length */
	lplen = ptr - leftpad;
	if (lplen)
		check_magic_number(leftpad, lplen);

	/* Right pad */
	rightpad = ptr + buflen;

	if (rplen)
		check_magic_number(rightpad, rplen);

	/* No need to unprotect the right guard since we are unmapping the whole
	 * thing */

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pfree: %p -->  "
			"|%lu|%lu|%lu|*%lu*|%lu|%lu|\n",
			ptr, ptr_off, 
			PAGE_SIZE, lplen, buflen, rplen, PAGE_SIZE);
#endif

	munmap(leftguard - ptr_off, ptr_off + lplen + buflen + 
			rplen + 2 * PAGE_SIZE);
}


/**
 * Protected malloc.
 * Generic format: 
 * |left_guard|leftpad|buffer|rightpad|right_guard|
 * PMALLOC_OVER  -> rightpad minimized, but may not be zero due to alignment
 * issues
 * PMALLOC_UNDER -> leftpad  = 0
 *
 * @param size Buffer size to allocate
 * @return Pointer to allocated user buffer
 */
void *pmalloc(size_t size)
{
	size_t real_size, lplen, rplen;
	void *ptr, *mapped;
	int e;

	/* Calculate pads for PMALLOC_OVER */
	rplen = 0;
	lplen = size & ~PAGE_MASK;
	if (lplen) {
		lplen = PAGE_SIZE - lplen;
		/* Pointer returned to user must be aligned */
		rplen = lplen & (PMALLOC_ALIGNMENT - 1);
		lplen -= rplen;
	}

	/* 2 pages needed as guards */
	real_size = size + lplen + rplen + 2 * PAGE_SIZE;

	if ((mapped = mmap(NULL, real_size, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED) {
		fprintf(stderr, "Error: pmalloc/mmap\n");
		return NULL;
	}
	/* User buffer */
	ptr = mapped + PAGE_SIZE + lplen;

	if (lplen)
		/* Insert magic number in the left pad */
		mark_magic_number(mapped + PAGE_SIZE, lplen);


#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmalloc: %lu -->  "
			"|0|%lu|%lu|*%lu*|%lu|%lu| %p\n",
			size, 
			PAGE_SIZE, lplen, size, rplen, PAGE_SIZE, ptr);
#endif

	if (rplen)
		/* Insert magic number in the right pad */
		mark_magic_number(ptr + size, rplen);

	/* Meta information set in left guard page */
	set_pbuffer_info(mapped, 0, size, rplen);

	if (mprotect(mapped, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		fprintf(stderr, "Error: pmalloc/mprotect left guard\n");
		goto release_mem;
	}

	if (mprotect(ptr + size + rplen, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		fprintf(stderr, "Error: pmalloc/mprotect right guard\n");
		goto revert_lguard;
	}

	return ptr;

revert_lguard:
	mprotect(ptr, PAGE_SIZE, PROT_READ|PROT_WRITE);
release_mem:
	munmap(ptr, real_size);
	errno = e;
	return NULL;
}


/**
 * Guarded posix_memalign()
 * Generic format is:
 * |left_guard|leftpad|aligned_buffer|rightpad|right_guard|
 * PMALLOC_OVER  -> rightpad minimized, but probable will not be zero to achieve
 * alignment
 * PMALLOC_UNDER -> leftpad  = 0
 *
 * @param ptr Allocated aligned buffer
 * @param alignment Requested alignment
 * @param size Size of requested buffer
 */
int pmemalign(void **ptr, size_t alignment, size_t size)
{
	void *mapped, *leftguard, *rightguard;
	size_t real_size, rplen, lplen;
	int e;

	/* Check that alignment is a power of two (Bit Twiddling hacks)
	 * Check that alignment is a multiple of sizeof(void *) */
	if (!(alignment && !(alignment & (alignment - 1))) 
			|| (alignment & (sizeof(void *) - 1))) {
		/* posix_memalign will fail */
		return EINVAL;
	}

	real_size = size + 
		((alignment > PAGE_SIZE)? alignment : PAGE_SIZE) + PAGE_SIZE;
	if (real_size & ~PAGE_MASK)
		/* Align to page size */
		real_size = (real_size & PAGE_MASK) + PAGE_SIZE; 

	if ((mapped = mmap(NULL, real_size, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED) {
		e = errno;
		fprintf(stderr, "Error: pmemalign/mmap\n");
		return e; /* Memory allocation failed */
	}

	/* Place buffer close to the guard page */
	*ptr = mapped + real_size - PAGE_SIZE - size;
	rplen = (unsigned long)*ptr & (alignment - 1);
	*ptr -= rplen; /* ptr is now aligned */

	/* Bytes to the beginning of the page */
	lplen = (unsigned long)*ptr & ~PAGE_MASK;

	/* Left guard is on the previous page */
	leftguard = *ptr - lplen - PAGE_SIZE;

	rightguard = *ptr + size + rplen;

	set_pbuffer_info(leftguard, 0, size, rplen);

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmemalign: %lu %lu -->  "
			"|%lu|%lu|*%lu*|%lu|%lu| %p\n",
			alignment, size,
			PAGE_SIZE, lplen, size, rplen, PAGE_SIZE, *ptr);
#endif

	/* Protect left guard page */
	if (mprotect(leftguard, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		goto release_mem;
	}

	/* Protect right guard page */
	if (mprotect(rightguard, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		goto release_mem;
	}

	/* Get rid of excess space on the left */
	if (mapped < leftguard)
		munmap(mapped, leftguard - mapped);

	/* Get rid of excess space on the right */
	if ((mapped + real_size) >  (rightguard + PAGE_SIZE))
		munmap(rightguard + PAGE_SIZE, 
			(rightguard + PAGE_SIZE) - (mapped + real_size));

	/* Insert magic number in the left pad */
	if (lplen)
		mark_magic_number(leftguard + PAGE_SIZE, lplen);

	/* Insert magic number in the right pad */
	if (rplen)
		mark_magic_number(rightguard - rplen, rplen);

	return 0;

release_mem:
	munmap(mapped, real_size);
	return e;
}

void pmalloc_init(void)
{
	error_page = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (error_page == MAP_FAILED) {
map_failed:
		generate_fault = raise_signal;
		perror("init/mmap");
	} else {
		generate_fault = access_error_page;
		mark_magic_number(error_page, PAGE_SIZE);
		if (mprotect(error_page, PAGE_SIZE, PROT_NONE) != 0)
			goto map_failed;
	}

#ifdef PMALLOC_WHITELIST
	whitelist_init();
#endif
}

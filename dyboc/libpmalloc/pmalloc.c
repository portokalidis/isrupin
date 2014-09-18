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
 2 |left left pad|left guard|left pad|buf|right pad|right guard|
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
	unsigned char *start, *end;

#if 0
	unsigned char magicc[4] = PMALLOC_MAGIC_NUMBER_STR;
	size_t counter = 0;

	for (start = (unsigned char *)ptr, end = (unsigned char *)ptr + len; 
			start < end; start++, counter++) {
		*start = magicc[counter & 3];

	}
#else
	/* XXX: This implementation causes an unexplained SEGFAULT when building
	 * with -O3, possible due to how gcc unrolls the loop. */
	uint32_t magic;
	for (start = (unsigned char *)ptr, end = (unsigned char *)ptr + len; 
			(start + 3) < end; start += 4) {
		*(uint32_t *)start = (uint32_t)PMALLOC_MAGIC_NUMBER;
	}
	for (magic = PMALLOC_MAGIC_NUMBER; start < end; start++, magic >>= 8) {
		*(uint8_t *)start = (uint8_t)magic;
	}
#endif

}

static void check_magic_number(void *ptr, size_t len)
{
	void *start, *end;
	uint32_t magic;

	/* Check that the magic values (canaries) are there */
	for (start = ptr, end = ptr + len; (start + 3) < end; start += 4) {
		if (*(uint32_t *)start != PMALLOC_MAGIC_NUMBER) {
			fprintf(stderr, "pmalloc: magic number "
					"does not match\n");
			generate_fault();
		}
	}
	for (magic = PMALLOC_MAGIC_NUMBER; start < end; start++, magic >>= 8) {
		if (*(uint8_t *)start != (uint8_t)magic) {
			fprintf(stderr, "pmalloc: magic number "
					"does not match\n");
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

/**
 * Protected realloc.
 *
 * @param memptr Pointer to previous buffer
 * @param size New required size
 * @return Pointer to reallocated user buffer
 */
void *prealloc(void *memptr, size_t size)
{
	void *leftguard, *rightpad, *leftpad, *newptr;
	size_t ptr_off, buflen, rplen, lplen, real_size;

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

	real_size = ptr_off + buflen + lplen + rplen + PAGE_SIZE;
#ifndef PMALLOC_UNDER
	/* Make right guard RW */
	if (mprotect(rightpad + rplen, PAGE_SIZE, PROT_READ|PROT_WRITE) != 0)
		generate_fault();
	real_size += PAGE_SIZE;
#endif

	newptr = pmalloc(size);
	if (!newptr)
		return newptr;
	memcpy(newptr, memptr, MIN(buflen, size));
	munmap(leftguard - ptr_off, real_size);
	return newptr;
}

/**
 * Free a pbuffer.
 * Unprotect guards and call the original free with main pointer.
 * leftguard is always at the page preceding the buffer and contains the
 * information saved with pbuffer_set_info().
 * Generic image of a pbuffer in memory:
 * |left left pad|left guard|left pad|buf|right pad|right guard|
 * PMALLOC_OVER  -> rightpad minimized, but may not be zero due to alignment
 * issues
 * PMALLOC_UNDER -> leftpad  = 0 AND !right_guard
 *
 * @param ptr Pointer to user buffer
 */
void pfree(void *ptr)
{
	void *leftguard, *rightpad, *leftpad;
	size_t ptr_off, buflen, rplen, lplen, real_size;

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

	real_size = ptr_off + lplen + buflen + rplen + PAGE_SIZE;
#if !defined(PMALLOC_UNDER)
	real_size += PAGE_SIZE;
#endif

	/* No need to unprotect the right guard since we are unmapping the whole
	 * thing */

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pfree: %p -->  "
			"|%lu|%lu|%lu|*%lu*|%lu|%lu|\n",
			ptr, ptr_off, 
			PAGE_SIZE, lplen, buflen, rplen, 
# ifdef PMALLOC_UNDER
			0LU
# else
			PAGE_SIZE
# endif
			);
#endif

	munmap(leftguard - ptr_off, real_size);
}


/**
 * Protected malloc.
 * Generic format: 
 * |left_guard|leftpad|buffer|rightpad|right_guard|
 * PMALLOC_OVER  -> rightpad minimized, but may not be zero due to alignment
 * issues
 * PMALLOC_UNDER -> leftpad  = 0 AND !right_guard
 *
 * @param size Buffer size to allocate
 * @return Pointer to allocated user buffer
 */
void *pmalloc(size_t size)
{
	size_t real_size, lplen, rplen;
	void *ptr, *mapped;
	int e;

#ifdef PMALLOC_UNDER
	lplen = 0; /* Page size is better than whatever PMALLOC_ALIGNMENT */
	rplen = size & ~PAGE_MASK;
	if (rplen)
		rplen = PAGE_SIZE - rplen;
#else
	/* Calculate pads for PMALLOC_OVER */
	rplen = 0;
	lplen = size & ~PAGE_MASK;
	if (lplen) {
		lplen = PAGE_SIZE - lplen;
		/* Pointer returned to user must be aligned */
		rplen = lplen & (PMALLOC_ALIGNMENT - 1);
		lplen -= rplen;
	}
#endif

	/* 1 page needed as from under flows */
	real_size = size + lplen + rplen + PAGE_SIZE;
#if !defined(PMALLOC_UNDER)
	real_size += PAGE_SIZE;
#endif

	if ((mapped = mmap(NULL, real_size, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED) {
		fprintf(stderr, "Error: pmalloc/mmap\n");
		return NULL;
	}
	/* User buffer */
	ptr = mapped + PAGE_SIZE + lplen;

	if (lplen) {
		/* Insert magic number in the left pad */
		mark_magic_number(mapped + PAGE_SIZE, lplen);
	}

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmalloc: %lu -->  "
			"|%lu|%lu|*%lu*|%lu|%lu| %p\n",
			size, 
			PAGE_SIZE, lplen, size, rplen, 
# ifdef PMALLOC_UNDER
			0LU,
# else
			PAGE_SIZE,
# endif
			ptr);
#endif

	/* Meta information set in left guard page */
	set_pbuffer_info(mapped, 0, size, rplen);

	if (mprotect(mapped, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		fprintf(stderr, "Error: pmalloc/mprotect left guard\n");
		goto release_mem;
	}

	if (rplen) {
		/* Insert magic number in the right pad */
		mark_magic_number(ptr + size, rplen);
	}

#if !defined(PMALLOC_UNDER)
	if (mprotect(ptr + size + rplen, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		fprintf(stderr, "Error: pmalloc/mprotect right guard\n");
		goto release_mem;
	}
#endif

	return ptr;

release_mem:
	munmap(mapped, real_size);
	errno = e;
	return NULL;
}


/**
 * Guarded posix_memalign()
 * Generic format is:
 * |left_guard|leftpad|aligned_buffer|rightpad|right_guard|
 * PMALLOC_OVER  -> rightpad minimized, but probable will not be zero to achieve
 * alignment
 * PMALLOC_UNDER -> leftpad  = 0 AND !right_guard
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

	real_size = size + ((alignment > PAGE_SIZE)? alignment : PAGE_SIZE);
#if !defined(PMALLOC_UNDER)
	real_size += PAGE_SIZE;
#endif
	if (real_size & ~PAGE_MASK)
		/* Align to page size */
		real_size = (real_size & PAGE_MASK) + PAGE_SIZE; 

	if ((mapped = mmap(NULL, real_size, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED) {
		e = errno;
		fprintf(stderr, "Error: pmemalign/mmap\n");
		return e; /* Memory allocation failed */
	}

#ifdef PMALLOC_UNDER
	/* Place buffer close to the left guard page */
	lplen = 0;
	*ptr = mapped + PAGE_SIZE;
	int align = (unsigned long)*ptr & (alignment - 1);
	if (align) {
		align = alignment - align;
		*ptr += align;
	}
	rplen = size & ~PAGE_MASK;
	if (rplen)
		rplen = PAGE_SIZE - rplen;
#else
	/* Place buffer close to the right guard page */
	*ptr = mapped + real_size - PAGE_SIZE - size;
	rplen = (unsigned long)*ptr & (alignment - 1);
	*ptr -= rplen; /* ptr is now aligned */
	/* Bytes to the beginning of the page */
	lplen = (unsigned long)*ptr & ~PAGE_MASK;
#endif

	/* Left guard is on the previous page */
	leftguard = *ptr - lplen - PAGE_SIZE;

	rightguard = *ptr + size + rplen;

	set_pbuffer_info(leftguard, 0, size, rplen);

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmemalign: %lu %lu -->  "
			"|%lu|%lu|*%lu*|%lu|%lu| %p\n",
			alignment, size,
			PAGE_SIZE, lplen, size, rplen, 
# ifdef PMALLOC_UNDER
			0LU,
# else
			PAGE_SIZE,
# endif
			*ptr);
#endif

	/* Protect left guard page */
	if (mprotect(leftguard, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		goto release_mem;
	}
	/* Get rid of excess space on the left */
	if (mapped < leftguard)
		munmap(mapped, leftguard - mapped);

#ifndef PMALLOC_UNDER
	/* Protect right guard page */
	if (mprotect(rightguard, PAGE_SIZE, PROT_NONE) != 0) {
		e = errno;
		goto release_mem;
	}
	/* Get rid of excess space on the right */
	if ((mapped + real_size) >  (rightguard + PAGE_SIZE))
		munmap(rightguard + PAGE_SIZE, 
			(rightguard + PAGE_SIZE) - (mapped + real_size));
#endif

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

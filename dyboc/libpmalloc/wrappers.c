#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pmalloc.h"


void *malloc(size_t size)
{
	if (size == 0)
		return NULL;
	return pmalloc(size);
}


void *calloc(size_t nmemb, size_t size)
{
	void *ptr;
	size_t total_size;

	total_size = nmemb * size;
	if (total_size == 0)
		return NULL;
	ptr = pmalloc(total_size);
	if (ptr)
		memset(ptr, 0, total_size);
	return ptr;
}

void free(void *ptr)
{
	if (ptr)
		pfree(ptr);
}

void *realloc(void *ptr, size_t size)
{
	if (!ptr) {
		if (size == 0)
			return NULL;
		else
			return pmalloc(size);
	} else if (size == 0) {
		pfree(ptr);
		return NULL;
	}

	return prealloc(ptr, size);
}


int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	/* We return NULL without an error */
	if (size == 0) {
		*memptr = NULL;
		return 0;
	}
	return pmemalign(memptr, alignment, size);
}

void *memalign(size_t alignment, size_t size)
{
#if 0
	int r;
	void *ptr;

	r = posix_memalign(&ptr, alignment, size);
	if (r != 0)
		return NULL;
	return ptr;
#endif
	printf("Unimplemented memalign()!\n");
	abort();
}

void *pvalloc(size_t size)
{
	printf("unimplemented pvalloc()!\n");
	abort();
}

void *valloc(size_t size)
{
	printf("unimplemented valloc()!\n");
	abort();
}

void *aligned_alloc(size_t alignment, size_t size)
{
	printf("Unimplemented aligned_alloc()!\n");
	abort();
}

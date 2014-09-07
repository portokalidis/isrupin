#include <stdio.h>

#include "pmalloc.h"


void *malloc(size_t size)
{
	if (size == 0)
		return NULL;
	return pmalloc(size);
}


void *calloc(size_t nmemb, size_t size)
{
	if ((nmemb * size) == 0)
		return NULL;
	return pcalloc(nmemb, size);
}

void free(void *ptr)
{
	if (ptr)
		pfree(ptr);
}

void *realloc(void *ptr, size_t size)
{
	fprintf(stderr, "realloc UNIMPLEMENTED!\n");
	return NULL;
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

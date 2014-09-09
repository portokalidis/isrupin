#include <stdio.h>
#include <stdlib.h>

#include "pmalloc.h"


static int wrappers_loaded = 0;

static void wrappers_init()
{
	/* Set on entry so calloc can distinguish that the call is due to dlsym
	 * in pmalloc_init() */
	wrappers_loaded = 1;
	pmalloc_init();
}

void *malloc(size_t size)
{
	if (!wrappers_loaded)
		wrappers_init();
	if (size == 0)
		return NULL;
	return pmalloc(size);
}


void *calloc(size_t nmemb, size_t size)
{
	if (!wrappers_loaded)
		wrappers_init();
	/* dlsym called by pmalloc_init() needs to call calloc. Returning NULL
	 * seems to allow it to complete */
	if ((nmemb * size) == 0 || !pmalloc_done)
		return NULL;
	return pcalloc(nmemb, size);
}

void free(void *ptr)
{
	if (!wrappers_loaded)
		wrappers_init();
	if (ptr)
		pfree(ptr);
}

void *realloc(void *ptr, size_t size)
{
	if (!wrappers_loaded)
		wrappers_init();
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
	if (!wrappers_loaded)
		wrappers_init();
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>

void *malloc(size_t size)
{
	static void * (*real_malloc)(size_t) = NULL;
	void *p;

	fprintf(stderr, "This my %s()\n", MYMALLOC);
	if (!real_malloc)
		real_malloc = dlsym(RTLD_NEXT, "malloc");

	p = real_malloc(size);
	return p;
}

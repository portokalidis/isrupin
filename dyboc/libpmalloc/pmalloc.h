#ifndef _PMALLOC_H_
#define _PMALLOC_H_

#include <stddef.h>

void *pmalloc(size_t size);

void pfree(void *ptr);

void *pcalloc(size_t nmemb, size_t size);
	
void *prealloc(void *ptr, size_t size);

int pmemalign(void **memptr, size_t alignment, size_t size);

void pmalloc_init(void);


//JIKK
//void dyboc_sig_handler(int sig);

#define PMALLOC_MAGIC_NUMBER	0xdeadbeef

#endif /* _PMALLOC_H_ */

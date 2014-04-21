#include <stdio.h>
#define __USE_GNU
#include <sys/mman.h>
#include <linux/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define FILE1 "zero_file"
#define FILE2 "one_file"


static void *map_file(const char *fn, void *addr, unsigned long *len)
{
	struct stat sbuf;
	int fd, r, flags;
	void *map;

	if ((fd = open(fn, O_RDWR)) < 0) {
error:
		perror("failed");
		return NULL;
	}

	r = fstat(fd, &sbuf);
	if (r != 0)
		goto error;

	flags = MAP_SHARED;
	if (addr != NULL)
		flags |= MAP_FIXED;
	map = mmap(addr, sbuf.st_size, PROT_READ|PROT_WRITE|PROT_EXEC,
			flags, fd, 0);
	if (map == MAP_FAILED)
		goto error;
	printf("Mapped file at 0x%0x:%lu\n", (unsigned)map, sbuf.st_size);
	*len = sbuf.st_size;
	((char *)map)[sbuf.st_size - 1] = '0';
	return map;
}

int main(void)
{
	void *map1, *remap1, *remap2, *remap3;
	unsigned long len1;
	int i;

	// Map and check
	if ((map1 = map_file(FILE1, NULL, &len1)) == NULL)
		return 1;
	for (i = 0; i < len1; i++) {
		if (((char *)map1)[i] != '0') {
			printf("Weird error at %d\n", i);
			return 1;
		}
	}
	//pause();

	// Check enlarging
	remap1 = mremap(map1, len1, len1 + 4096, MREMAP_MAYMOVE);
	printf("Enlarged map to %8p:%lu\n", remap1, len1 + 4096);
	//pause();
	for (i = 0; i < (len1 + 4096); i++) {
		if (((char *)remap1)[i] != '0') {
			printf("Found byte that does not "
					"belong to map at %d\n", i);
			break;
		}
	}

	// Check shrinking
	remap2 = mremap(remap1, len1 + 4096, len1 - 4096, MREMAP_MAYMOVE);
	printf("Shrunk map to %8p:%lu\n", remap2, len1 - 4096);
	//pause();
	for (i = 0; i < (len1 - 4096); i++) {
		if (((char *)remap2)[i] != '0') {
			printf("Found byte that does not "
					"belong to map at %d\n", i);
			break;
		}
	}

	// Check partial remmaping
	remap3 = mremap(remap2, 4096, 4096, MREMAP_MAYMOVE|MREMAP_FIXED, 
			(void *)0x70000000);
	printf("Remapped first page to %8p\n", remap3);
	//pause();
	for (i = 0; i < 4096; i++) {
		if (((char *)remap3)[i] != '0') {
			printf("Found byte that does not "
					"belong to map at %d\n", i);
			break;
		}
	}

	return 0;
}

#include <stdio.h>
#include <sys/mman.h>
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

	flags = MAP_PRIVATE;
	if (addr != NULL)
		flags |= MAP_FIXED;
	map = mmap(addr, sbuf.st_size, PROT_READ|PROT_WRITE|PROT_EXEC,
			flags, fd, 0);
	if (map == MAP_FAILED)
		goto error;
	printf("Mapped file at 0x%0x:%lu\n", (unsigned)map, sbuf.st_size);
	*len = sbuf.st_size;
	return map;
}

int main(void)
{
	void *map1, *map2;
	unsigned long len1, len2;
	int i;

	if ((map1 = map_file(FILE1, NULL, &len1)) == NULL)
		return 1;
	if ((map2 = map_file(FILE2, (char *)map1 + 4096, &len2)) == NULL)
		return 1;

	for (i = 0; i < len1; i++) {
		if (((char *)map1)[i] != '0') {
			printf("Found that byte %d belongs to file2 (%c)\n",
					i, ((char *)map1)[i]);
			pause();
			return 0;
		}
	}
	printf("Second map had no effect\n");

	return 0;
}

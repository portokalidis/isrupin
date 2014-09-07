#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


int memtouch(void *buf, size_t len)
{
	int i = 0;
	size_t count;

	for (i = count = 0; i < len; i++, buf++)
		if (*(unsigned char *)buf == 'A')
			count ++;
	return count;
}


int main()
{
	int r;
	void *buf;

	if ((r = posix_memalign(&buf, 1024, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	memset(buf, 'A', 1008);
	free(buf);

	if ((r = posix_memalign(&buf, 8, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	memset(buf, 'A', 1008);
	memtouch(buf, 4096);
	free(buf);

	if ((r = posix_memalign(&buf, 4096, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	memset(buf, 'A', 1008);
	free(buf);

	if ((r = posix_memalign(&buf, 8192, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	memset(buf, 'A', 1008);
	free(buf);

	if ((r = posix_memalign(&buf, 1024, 80000)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	memset(buf, 'A', 80000);
	free(buf);

	return 0;
}


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	int r;
	void *buf;

	if ((r = posix_memalign(&buf, 1024, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	free(buf);

	if ((r = posix_memalign(&buf, 8, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	free(buf);

	if ((r = posix_memalign(&buf, 4096, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	free(buf);

	if ((r = posix_memalign(&buf, 8192, 1008)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	free(buf);

	if ((r = posix_memalign(&buf, 1024, 80000)) != 0) {
		fprintf(stderr, "memalign: %s\n", strerror(r));
		return 1;
	}
	free(buf);

	return 0;
}


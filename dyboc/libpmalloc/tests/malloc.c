#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


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
	void *buf;

	if ((buf = malloc(1008)) == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(ENOMEM));
		return 1;
	}
	memset(buf, 'A', 1008);
	free(buf);

	if ((buf = malloc(4096)) == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(ENOMEM));
		return 1;
	}
	memset(buf, 'A', 4096);
	free(buf);

	if ((buf = malloc(1)) == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(ENOMEM));
		return 1;
	}
	memset(buf, 'A', 1);
	memtouch(buf - 4095, 4096);
	free(buf);



	return 0;
}


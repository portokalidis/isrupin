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

	if ((buf = realloc(NULL, 1008)) == NULL) {
		fprintf(stderr, "realloc: %s\n", strerror(ENOMEM));
		return 1;
	}
	memset(buf, 'A', 1008);

	if ((buf = realloc(buf, 3000)) == NULL) {
		fprintf(stderr, "realloc: %s\n", strerror(ENOMEM));
		return 1;
	}
	memset(buf, 'A', 3000);

	if ((buf = realloc(buf, 4097)) == NULL) {
		fprintf(stderr, "realloc: %s\n", strerror(ENOMEM));
		return 1;
	}
	memset(buf, 'A', 4097);

	buf = realloc(buf, 0);

	return 0;
}


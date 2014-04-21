#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define ITEMS	1
#define SIZE	10000

int main(void)
{
	int i;
	void *buf[ITEMS];

	printf("Hello\n");

	srand(time(NULL));

	for (i = 0; i < ITEMS; i++) {
		buf[i] = malloc(SIZE);
	}

	for (i = 0; i < ITEMS; i++) {
		memset(buf[i], rand(), SIZE);
	}

	for (i = 0; i < ITEMS; i++) {
		free(buf[i]);
	}

	return 0;
}

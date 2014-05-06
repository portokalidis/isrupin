#include <stdio.h>
#include <stdlib.h>

void run_child(int n)
{
	char *p;
	int i;

	printf("Write %d chars\n", n);
	p = malloc(1000);
	for (i = 0; i < n; i++)
		p[1000 - i - 1] = 'A';

	printf("Writing at %p done\n", p);
#if 0
	for (i = 0; i < n; i++)
		if (p[i] != 'A')
			abort();
#endif
	free(p);
}

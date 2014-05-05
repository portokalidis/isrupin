#include <stdlib.h>
#include <stdio.h>


static void run_child(int n)
{
	char *p;
	int i;

	printf("Write %d chars\n", n);
	p = malloc(1000);
	for (i = 0; i < n; i++)
		p[i] = 'A';

	printf("Writing at %p done\n", p);
#if 0
	for (i = 0; i < n; i++)
		if (p[i] != 'A')
			abort();
#endif
	free(p);
}

int main(int argc, char **argv)
{
	int n;

	n = atoi(argv[1]);
	run_child(n);
	return EXIT_SUCCESS;
}

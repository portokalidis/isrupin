#include <stdlib.h>
#include <stdio.h>

void run_child(int n);

int main(int argc, char **argv)
{
	int n;

	n = atoi(argv[1]);
	run_child(n);
	return EXIT_SUCCESS;
}

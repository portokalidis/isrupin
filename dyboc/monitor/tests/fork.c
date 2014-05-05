#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
	pid_t pid;
	int status, n;

	printf("TEST: Process running\n");
	pid = fork();
	if (pid == 0) {
		printf("TEST: child running\n");
		n = atoi(argv[1]);
		run_child(n);
	} else if (pid > 0) {
		printf("TEST: parent waiting\n");
		wait(&status);
		printf("TEST: parent exiting\n");
	} else {
		perror("fork");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

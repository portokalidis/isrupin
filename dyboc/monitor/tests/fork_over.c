#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>


void run_child(int n);

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
		printf("TEST: child exiting\n");
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

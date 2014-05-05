#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	pid_t pid;
	int status;

	printf("TEST: Process running\n");
	pid = fork();
	if (pid == 0) {
		printf("TEST: child running\n");
		if (execvp(argv[1], argv + 1) != 0) {
			perror("exec");
			return EXIT_FAILURE;
		}
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

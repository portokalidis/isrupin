#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>


static int run_child()
{
	int i, charcount;
	FILE *fl;
	char line[1024];

	printf("Child: will attempt to close all sockets\n");
	for (i = 0; i < 65536; i++) {
		close(i);
	}

	fl = fopen("/etc/passwd", "r");
	if (!fl)
		return -1;
	charcount = 0;
	while (fgets(line, sizeof(line), fl) != NULL) {
		line[sizeof(line) - 1] = '\0';
		charcount += strlen(line);
	}
	fclose(fl);

	exit(10000);
	return charcount;
}

static void run_parent(FILE *fl)
{
	int charcount;
	char line[1024];

	charcount = 0;
	while (fgets(line, sizeof(line), fl) != NULL) {
		line[sizeof(line) - 1] = '\0';
		charcount += strlen(line);
		printf("Parent %d: %s", charcount, line);
	}
	printf("Parent: total chars %d\n", charcount);
}

int main()
{
	FILE *fl;
	pid_t p;
	int status;

	fl = fopen("/etc/passwd", "r");
	if (!fl) {
		perror("fopen");
		return 1;
	}

	printf("Parent: opened /etc/passwd\n");

	if ((p = fork()) == 0) {
		run_child();
	} else if (p > 0) {
		run_parent(fl);
		printf("Parent: waiting for child\n");
		status = 0;
		wait(&status);
		printf("Child exited with %d\nParent: exiting\n", 
				WEXITSTATUS(status));
	} else {
		perror("fork");
		return 1;
	}
	return 0;
}

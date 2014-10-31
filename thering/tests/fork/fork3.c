#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>


static int run_child()
{
	int charcount;
	FILE *fl;
	char line[1024];

	fl = fopen("/etc/passwd", "r");
	if (!fl)
		return -1;
	charcount = 0;
	while (fgets(line, sizeof(line), fl) != NULL) {
		line[sizeof(line) - 1] = '\0';
		charcount += strlen(line);
	}
	fclose(fl);
	printf("CHILD read: %d chars\n", charcount);

	*((char *)((long)line | ~0xfffffff))  = (char)'a';

	return 0xc;
}

static void run_parent(FILE *fl)
{
	int charcount;
	char line[1024];

	charcount = 0;
	while (fgets(line, sizeof(line), fl) != NULL) {
		line[sizeof(line) - 1] = '\0';
		charcount += strlen(line);
		//printf("Parent %d: %s", charcount, line);
	}
	printf("PARENT read: %d chars\n", charcount);

	*((char *)((long)line | ~0xfffffff))  = (char)'a';
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
		printf("CHILD: %d\n", getpid());
		return run_child();
	} else if (p > 0) {
		printf("PARENT: %d\n", getpid());
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

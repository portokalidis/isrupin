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

	return 0;
}

int main()
{
	int i;
	pid_t p;
	int status;

	printf("Parent: will attempt to close all sockets\n");
	for (i = 0; i < 65536; i++) {
		close(i);
	}

	if ((p = fork()) == 0) {
		run_child();
	} else if (p > 0) {
		status = 0;
		wait(&status);
	} else {
		return 1;
	}
	return 0;
}

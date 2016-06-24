#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>


static void run_child()
{
	printf("Child sleep for 30 seconds\n");
	sleep(30);
	printf("Child wake up\n");
}

static void run_parent()
{
	printf("Parent sleep for 30 seconds\n");
	sleep(30);
	printf("Parent wake up\n");
}

int main()
{
	pid_t p;
	int status;

	printf("Sleep for 10 seconds\n");
	sleep(10);
	printf("Wake up\n");

	if ((p = fork()) == 0) {
		run_child();
	} else if (p > 0) {
		run_parent();
		wait(&status);
		printf("Child exited with %d\nParent: exiting\n", 
				WEXITSTATUS(status));
	} else {
		perror("fork");
		return 1;
	}
	return 0;
}

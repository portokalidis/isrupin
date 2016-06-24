#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
	printf("Sleep for 5 seconds\n");
	sleep(5);
	printf("Wake up and execute myself\n");

	execv(argv[0], argv);
	perror("execv");
	return 1;
}

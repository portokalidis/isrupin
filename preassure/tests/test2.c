#include <stdio.h>
#include <time.h>

int main(int arch, char **argv)
{
	time_t tm;

	tm = time(NULL);
	
	printf("Hello world at %lu\n", tm);

	if ((tm % 2) == 0)
		return 1;
	else
		return 0;
}

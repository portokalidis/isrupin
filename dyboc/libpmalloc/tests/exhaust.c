#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/user.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#define BLOCK_SIZE (1024)
#define LIMIT (2UL * 1024 * 1024 * 1024)
//#define LIMIT (2UL * 1024)

static void print_usage(unsigned long long allocated, 
		unsigned long long total, unsigned long long maps)
{
	fprintf(stderr, "Allocated %.2f MB (%.2f KB)\n"
			"Total %.2f MB (%.2f KB)\n"
			"Ratio %.2f %%\n"
			"Maps %llu\n",
			allocated / (double)(1024 * 1024),
			allocated / (double)(1024),
			total / (double)(1024 * 1024),
			total / (double)(1024),
			allocated / (double)total * 100,
			maps);
}

static void fill_random(void *buf, size_t len)
{
	unsigned char *start_p = (unsigned char *)buf;
	unsigned char byte = rand() & 0xff;
	unsigned char *end_p = start_p + len;

	for (; start_p < end_p; start_p++)
		*start_p = byte;
}

int main()
{
	void *buf;
	struct rlimit as_limit;
	unsigned long long allocated = 0, total_expected = 0, maps = 0;

	srand(time(NULL));

	printf("PID = %d\n", getpid());

	if (getrlimit(RLIMIT_AS, &as_limit) != 0) {
		perror("getrlimit");
		return 1;
	}

	printf("RLIMIT_AS %lu/%lu\n", as_limit.rlim_cur, as_limit.rlim_max);

	while (total_expected < LIMIT) {
		if ((buf = malloc(BLOCK_SIZE)) == NULL) {
			fprintf(stderr, "malloc: %s\n", strerror(ENOMEM));
			goto ret;
		}
		allocated += BLOCK_SIZE;
		total_expected += 2 * PAGE_SIZE + (BLOCK_SIZE & PAGE_MASK) +
			((BLOCK_SIZE & ~PAGE_MASK)? PAGE_SIZE : 0);
		maps ++;

		fill_random(buf, BLOCK_SIZE);
	}

ret:
	print_usage(allocated, total_expected, maps);
	pause();
	return 0;
}


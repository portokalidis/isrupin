#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/user.h>
#include <time.h>

#define BLOCK_SIZE 1024
#define LIMIT (2UL * 1024 * 1024 * 1024)
//#define LIMIT (2UL * 1024)

static void print_usage(unsigned long long allocated, unsigned long long total)
{
	fprintf(stderr, "Allocated %.2f MB (%.2f KB)\n"
			"Total %.2f MB (%.2f KB)\n"
			"Ratio %.2f %%\n",
			allocated / (double)(1024 * 1024),
			allocated / (double)(1024),
			total / (double)(1024 * 1024),
			total / (double)(1024),
			allocated / (double)total * 100);
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
	unsigned long long allocated = 0, total_expected = 0;

	srand(time(NULL));

	printf("PID = %d\n", getpid());

	while (total_expected < LIMIT) {
		if ((buf = malloc(BLOCK_SIZE)) == NULL) {
			fprintf(stderr, "malloc: %s\n", strerror(ENOMEM));
			goto ret;
		}
		allocated += BLOCK_SIZE;
		total_expected += 2 * PAGE_SIZE + (BLOCK_SIZE & PAGE_MASK) +
			((BLOCK_SIZE & ~PAGE_MASK)? PAGE_SIZE : 0);

		fill_random(buf, BLOCK_SIZE);
	}

ret:
	print_usage(allocated, total_expected);
	pause();
	return 0;
}


#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>

/* exit 1 shellcode */
const char payload[] = "\x31\xc0\xb0\x01\xcd\x80";


int main(int argc, const char *argv[])
{
	void *bad_page;
	void (*payload_func)(void);

	bad_page = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC, 
			MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (bad_page == MAP_FAILED) {
		perror("memory map failed");
		return EXIT_FAILURE;
	}

	memcpy(bad_page, payload, sizeof(payload));

	payload_func = (void (*)())bad_page;
	payload_func();

	return EXIT_SUCCESS;
}

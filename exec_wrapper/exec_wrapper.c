#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ISR_HOME	"ISR_HOME"
#define LD_LIB_PATH	"LD_LIBRARY_PATH"


int main(int argc, char **argv)
{
	char *ldpath, *newldpath, *isr_home;

	if (argc < 2) {
		fprintf(stderr, "Not enough arguments\n");
		return -1;
	}

	/* Check that ISR_HOME is defined */
	isr_home = getenv(ISR_HOME);
	if (!isr_home) {
		fprintf(stderr, "ISR_HOME not defined!\n");
		return -1;
	}
	//fprintf(stderr,"%s=%s\n", ISR_HOME, isr_home);

	/* We need to alter LD_LIBRARY_PATH to include the encrypted libraries
	 * first */
	ldpath = getenv(LD_LIB_PATH);
	if (ldpath) {
		newldpath = malloc(strlen(isr_home) + strlen(ldpath) + 16);
		assert(newldpath);
		sprintf(newldpath, "%s/db/encrypted_lib:%s", isr_home, ldpath);
	} else {
		newldpath = malloc(strlen(isr_home) + 16);
		assert(newldpath);
		sprintf(newldpath, "%s/db/encrypted_lib", isr_home);
	}
	//fprintf(stderr,"%s=%s\n", LD_LIB_PATH, newldpath);
	setenv(LD_LIB_PATH, newldpath, 1);

#if 0
	for (i = 1; i < argc; i++)
		printf("argv[%d] = %s\n", i, argv[i]);
#endif

	//printf("Wrapper executing process %s\n", argv[1]);
	execv(argv[1], argv + 1);
	perror("Execution failed");
	return -1;
}

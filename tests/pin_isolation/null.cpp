#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "pin.H"

static FILE *fp = NULL;
static int private_integer = 100;


VOID Fini(INT32 code, VOID *v)
{
	fprintf(fp, "My private_integer at 0x%08x value %d\n", 
			(unsigned int)&private_integer, private_integer);
	fclose(fp);
}


int main(int argc, char *argv[])
{
	if (!(fp = fopen("null.log", "w"))) {
		perror("cannot open file");
		return -1;
	}

	fprintf(fp, "My private_integer at 0x%08x value %d\n", 
			(unsigned int)&private_integer, private_integer);
	fflush(fp);

	// Initialize pin
	PIN_Init(argc, argv);

	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}

#include <stdio.h>



static int main_function()
{
	fprintf(stderr, "What a crazy world it is\n");
	return 100;
}

int main()
{
	return main_function();
}

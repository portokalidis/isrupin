#include <stdio.h>
#include "pmalloc.h"
#include <unistd.h>
#include <errno.h>

int main()
{
	char *buf;
	buf = pmalloc_2(10, __FILE__, __FUNCTION__, __LINE__, "main");
	//a2f_show_objects();

	pfree(buf);
	return 0;    
}


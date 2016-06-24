#include <stdio.h>
#include <stdlib.h>

int main()
{
	char *tmp, *p;
	int i;

	tmp = malloc(10024);

	for (i = 0; i < 10024; i++)
		tmp[i] = 'A';

	p = (char *)0xb749e000;
	*p = 'A';

	return 0;
}

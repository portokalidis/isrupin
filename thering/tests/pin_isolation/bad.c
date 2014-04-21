#include <stdio.h>

int main(void)
{
	unsigned int *addr;

	printf("Insert target address(0x....):");
	scanf("0x%08x", (unsigned int *)&addr);

	printf("Overwriting address 0x%08x\n", (unsigned int)addr);
	*addr = 666;

	return 0;
}

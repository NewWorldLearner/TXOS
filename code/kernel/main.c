#include "include/print_kernel.h"
#include "include/interrupt.h"
#include "include/debug.h"
#include "include/memory.h"

extern struct Global_Memory_Descriptor memory_management_struct;

void Start_Kernel(void)
{
    init_screen();
	printf("test %x\n", 255);
	printf("test number %d\n", 255);
	printf("test string %s", "this is a string\n");
	printf("test int 0: %d\n", 0);
	init_memory();
	while (1)
		;
}
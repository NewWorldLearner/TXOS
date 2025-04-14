#include "print_kernel.h"
#include "interrupt.h"
#include "lib/debug.h"

void idt_init();

void Start_Kernel(void)
{
    init_screen();
	printf("test %x\n", 255);
	printf("test number %d\n", 255);
	printf("test string %s", "this is a string\n");
	// ASSERT(1==2);
    while (1)
        ;
}

#include "include/print_kernel.h"
#include "include/interrupt.h"
#include "include/debug.h"
#include "include/memory.h"

#include "include/8259A.h"

extern struct Global_Memory_Descriptor memory_management_struct;

void Start_Kernel(void)
{
    init_screen();
	init_memory();
	idt_init();
	init_memory_slab();
	pagetable_init();
	pic_8259A_init();

	while (1)
		;
}
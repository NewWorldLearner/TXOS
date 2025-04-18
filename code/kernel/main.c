#include "include/print_kernel.h"
#include "include/interrupt.h"
#include "include/debug.h"
#include "include/memory.h"

#if APIC
	#include "include/APIC.h"
#else
	#include "include/8259A.h"
#endif

extern struct Global_Memory_Descriptor memory_management_struct;

void Start_Kernel(void)
{
    init_screen();
	init_memory();
	idt_init();
	init_memory_slab();
	pagetable_init();

	#if APIC
		LAPIC_IOAPIC_init();
	#else
		pic_8259A_init();
	#endif

	while (1)
		;
}
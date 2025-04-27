#include "include/print_kernel.h"
#include "include/interrupt.h"
#include "include/debug.h"
#include "include/memory.h"
#include "include/keyboard.h"
#include "include/mouse.h"
#include "include/disk.h"
#include "include/timer.h"
#include "include/thread.h"

#if APIC
	#include "include/APIC.h"
#else
	#include "include/8259A.h"
#endif

extern struct Global_Memory_Descriptor memory_management_struct;


uint64_t init(uint64_t arg)
{
	struct pt_regs *regs;

	printf("init task is running,arg:%x\n", arg);
	while (1)
		;
}

void Start_Kernel(void)
{
    init_screen();
	init_memory();
	idt_init();
	//init_memory_slab();
	pagetable_init();

	#if APIC
		LAPIC_IOAPIC_init();
	#else
		pic_8259A_init();
	#endif

	keyboard_init();

	mouse_init();

	// disk_init();

	// char buff[512];
	// IDE_transfer(ATA_READ_CMD,0,1,buff);
	// printf("LBA 0 sector:%s\n",buff);

	//timer_init();

	kernel_process_init();

	thread_create(init, 0);

	schedule();

	while (1)
		;
}
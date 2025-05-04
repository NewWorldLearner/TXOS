#include "include/print_kernel.h"
#include "include/interrupt.h"
#include "include/debug.h"
#include "include/memory.h"
#include "include/keyboard.h"
#include "include/mouse.h"
#include "include/disk.h"
#include "include/timer.h"
#include "include/thread.h"
#include "include/process.h"

#if APIC
#include "include/APIC.h"
#else
#include "include/8259A.h"
#endif

extern struct Global_Memory_Descriptor memory_management_struct;

// init进程现在是用户进程，它位于特权级3下面，按理来说是无法访问到内核代码的，但是我们暂且把内核页表的权限设置了用户可读写
// 因此，所以才能够调用位于内核空间的printf函数
uint64_t init(uint64_t arg)
{
	printf("I am user program\n");
	while (1)
		;
}

void Start_Kernel(void)
{
	init_screen();
	init_memory();
	idt_init();
	init_memory_slab();
	// pagetable_init();

#if APIC
	LAPIC_IOAPIC_init();
#else
	pic_8259A_init();
#endif

	keyboard_init();

	mouse_init();

	// disk_init();

	// timer_init();


	init_memory_pool();
	kernel_process_init();
	// thread_create(init, 0);
	process_creat((uint64_t*)init);
	schedule();

	// uint64_t vaddr = (uint64_t)get_vaddr(PF_KERNEL, 1, PG_Kernel);
	// // struct Page *p = alloc_pages(ZONE_NORMAL, 1, PG_Kernel);
	// // uint64_t vaddr = Phy_To_Virt(p->PHY_address);
	// printf("kernel vaddr:0x%x\n", vaddr);


	while (1)
		;
}
#include "include/print_kernel.h"
#include "include/interrupt.h"
#include "include/debug.h"
#include "include/memory.h"

extern struct Global_Memory_Descriptor memory_management_struct;

void Start_Kernel(void)
{
    init_screen();
	init_memory();
	struct Page *page = alloc_pages(ZONE_DMA, 1, 0);
	printf("alloc a page, address:0x%x\n", page->PHY_address);

	page = alloc_pages(ZONE_DMA, 1, 0);
	printf("alloc a page, address:0x%x\n", page->PHY_address);

	free_pages(page, 1);
	printf("free a page, address:0x%x\n", page->PHY_address);
	
	page = alloc_pages(ZONE_DMA, 1, 0);
	printf("alloc a page, address:0x%x\n", page->PHY_address);


	// 测试中断
	idt_init();

	printf("init memory slab descripter\n");
	init_memory_slab();
	printf("init memory slab descripter end\n");

	char *str = kmalloc(32, 0);
	printf("malloc memory block address:%x\n",str);

	char *str1 = kmalloc(32, 0);
	printf("malloc memory block address:%x\n", str1);

	kfree(str1);

	char *str2 = kmalloc(32, 0);
	printf("malloc memory block address:%x\n", str2);

	// 测试页表的初始化
	printf("init pagetable\n");
	pagetable_init();
	printf("init pagetable end\n");

	while (1)
		;
}
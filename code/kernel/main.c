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

	// init_memory_slab似乎存在bug
	// printf("init memory slab descripter\n");
	// init_memory_slab();
	// printf("init memory slab descripter end\n");

	// // kmalloc 似乎存在缺页异常的bug，待修复
	// char *str = kmalloc(32, 0);
	// printf("malloc memory block address:%x\n",1);

	while (1)
		;
}
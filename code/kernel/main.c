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

	while (1)
		;
}
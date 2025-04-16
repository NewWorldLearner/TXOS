#include "include/memory.h"
#include "include/string.h"
#include "include/debug.h"
#include "include/print_kernel.h"

#define NULL (void *)0

extern char _text;
extern char _etext;
extern char _edata;
extern char _end;

struct Global_Memory_Descriptor memory_management_struct = {{0}, 0};

int ZONE_DMA_INDEX = 0;
int ZONE_NORMAL_INDEX = 0;  // low 1GB RAM ,was mapped in pagetable
int ZONE_UNMAPED_INDEX = 0; // above 1GB RAM,unmapped in pagetable

uint64_t *Global_CR3 = NULL;

uint64_t page_init(struct Page *page, uint64_t flags)
{
    page->attribute |= flags;

    if (!page->reference_count || (page->attribute & PG_Shared))
    {
        page->reference_count++;
        page->zone_struct->total_pages_link++;
    }

    return 1;
}

uint64_t page_clean(struct Page *page)
{
    page->reference_count--;
    page->zone_struct->total_pages_link--;

    if (!page->reference_count)
    {
        page->attribute &= PG_PTable_Maped;
    }

    return 1;
}

uint64_t get_page_attribute(struct Page *page)
{
    ASSERT(page != NULL);
    return page->attribute;
}

uint64_t set_page_attribute(struct Page *page, uint64_t flags)
{
    ASSERT(page != NULL);
    page->attribute = flags;
    return 1;
}





// --------------------------------------------------------------------------
// ------------下面的几个静态函数都是为了辅助init_memory这个函数的实现-----------
// --------------------------------------------------------------------------

// 必须要在内存描述符初始化完成之后才能够调用该函数
static uint64_t calculate_total_memory()
{
    return memory_management_struct.e820[memory_management_struct.e820_length].address + memory_management_struct.e820[memory_management_struct.e820_length].length;
}

// 初始化内存描述符
static void init_memory_descriptors()
{
    struct E820 *p = (struct E820 *)0xffff800000007e20;
    uint64_t count = *(uint64_t *)0xffff800000007e00;
    // 记录ARDS结构体的最大偏移量
    memory_management_struct.e820_length = count - 1; // 这里曾经因为手误写成了memory_management_struct.e820->length
    printf("ARDS number:%d\n", count);
    // 让global_memory_desciptor记录0x7E20开始处的ARDS结构体信息
    for (int i = 0; i < count; i++)
    {
        memory_management_struct.e820[i] = *p++;
        printf("e820 %d address %x, length %d\n", i, memory_management_struct.e820[i].address, memory_management_struct.e820[i].length);
    }
    // 这里我们可以计算并打印出操作系统可用的2MB物理页的数量作为提示
    // 但是我这里不打印
}

// 初始化物理内存的描述位图
static void init_memory_bitmap()
{
    // 最大可用的物理内存
    int TotalMem = calculate_total_memory();

    // --------------------下面进行物理页映射位图的初始化------------------------

    // 位图，用于标识物理页，放在程序的结尾处的向上取整4KB的开始处
    memory_management_struct.bits_map = (uint64_t *)((memory_management_struct.end_brk + PAGE_4K_SIZE - 1) & PAGE_4K_MASK);

    // 位图中的可用位的总数，内存结尾处不足2MB的内存直接丢弃不使用
    memory_management_struct.bits_size = TotalMem >> PAGE_2M_SHIFT;

    // 对于位图占用的字节数，进行向上取整8字节对齐
    memory_management_struct.bits_length = (((uint64_t)(TotalMem >> PAGE_2M_SHIFT) + sizeof(long) * 8 - 1) / 8) & (~(sizeof(long) - 1));

    // 将位图中的位全部置为1
    memset(memory_management_struct.bits_map, 0xff, memory_management_struct.bits_length);
}

// 初始化page结构体数组
static void init_memory_pages()
{
    //---------------------------下面进行Page数组的初始化-----------------------

    // 页结构体数组安排在位图结束后的向上取整4KB对齐起始处
    memory_management_struct.pages_struct = (struct Page *)(((uint64_t)memory_management_struct.bits_map + memory_management_struct.bits_length + PAGE_4K_SIZE - 1) & PAGE_4K_MASK);
    uint64_t total_memory = calculate_total_memory();
    printf("total memory: %d M\n", total_memory / 1024 / 1024);
    // 页结构体数量，内存结尾处不足2MB的内存直接丢弃不使用
    memory_management_struct.pages_size = total_memory >> PAGE_2M_SHIFT;
    printf("page count:%d\n", memory_management_struct.pages_size);
    // 所有页结构体占的总字节长度，向上取整进行8字节对齐
    memory_management_struct.pages_length = ((total_memory >> PAGE_2M_SHIFT) * sizeof(struct Page) + sizeof(long) - 1) & (~(sizeof(long) - 1));
    printf("page struct end %x\n", memory_management_struct.pages_length);
    // 将页结构体数组中的所有数据清0
    memset(memory_management_struct.pages_struct, 0x00, memory_management_struct.pages_length);
}

// 初始化zone结构体数组
static void init_memory_zones()
{
    // --------------------------下面进行Zone数组的初始化------------------------

    // zone结构体安排在页结构体结束后的下一个4KB起始处
    memory_management_struct.zones_struct = (struct Zone *)(((uint64_t)memory_management_struct.pages_struct + memory_management_struct.pages_length + PAGE_4K_SIZE - 1) & PAGE_4K_MASK);

    // zone结构体的数量是未知的？？？
    memory_management_struct.zones_size = 0;

    // 所有zone结构体占的字节长度，这里暂且假设只有5个zone结构体
    memory_management_struct.zones_length = (5 * sizeof(struct Zone) + sizeof(long) - 1) & (~(sizeof(long) - 1));

    memset(memory_management_struct.zones_struct, 0x00, memory_management_struct.zones_length);
}

static void process_zone(struct E820 *entry)
{
    uint64_t start = PAGE_2M_UP_ALIGN(entry->address);
    uint64_t end = PAGE_2M_DOWN_ALIGN(entry->address + entry->length);

    if (end <= start)
        return;

    // 记录zone的个数，注意zones_size的初始值为0
    struct Zone *zone = &(memory_management_struct.zones_struct[memory_management_struct.zones_size++]);
    // 记录zone的起始物理地址
    zone->zone_start_address = start;
    // 记录zone的结束物理地址
    zone->zone_end_address = end;
    // 记录zone的字节数
    zone->zone_length = end - start;
    // zone用过的物理页为0
    zone->page_using_count = 0;
    // zone可使用的物理页
    zone->page_free_count = (end - start) >> PAGE_2M_SHIFT;
    zone->total_pages_link = 0;
    zone->attribute = 0;
    zone->GMD_struct = &memory_management_struct;
    // zone中页的数量
    zone->pages_length = zone->page_free_count;
    // zone中page结构体的起始地址
    zone->pages_group = &(memory_management_struct.pages_struct[start >> PAGE_2M_SHIFT]);

    // ----------------对zone中对应的page进行初始化----------------------
    struct Page *page = zone->pages_group;

    for (int j = 0; j < zone->pages_length; j++, page++)
    {
        page->zone_struct = zone;
        page->PHY_address = start + PAGE_2M_SIZE * j;
        page->attribute = 0;

        page->reference_count = 0;

        page->create_time = 0;
        // 求物理页对应的偏移量，单位8字节，然后将物理页对64取模，计算得到对应的位进行异或运算，其实就是把对应位置0
        *(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) ^= 1UL << ((page->PHY_address >> PAGE_2M_SHIFT) % 64);
    }
}

static void mark_used_pages()
{
    /* 标记内核已使用的物理页 */
    uint64_t kernel_end = Virt_To_Phy(memory_management_struct.end_of_struct);
    int pages_used = kernel_end >> PAGE_2M_SHIFT;

    // 被内核使用过的页，都对其进行初始化，也就是标记其使用过
    for (int i = 1; i <= pages_used; i++)
    {
        struct Page *page = &memory_management_struct.pages_struct[i];
        page_init(page, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
        *(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
        page->zone_struct->page_using_count++;
        page->zone_struct->page_free_count--;
    }
}

void init_memory()
{
    memory_management_struct.start_code = (uint64_t)&_text;
    memory_management_struct.end_code = (uint64_t)&_etext;
    memory_management_struct.end_data = (uint64_t)&_edata;
    memory_management_struct.end_brk = (uint64_t)&_end;

    init_memory_descriptors();

    init_memory_bitmap();

    init_memory_pages();

    init_memory_zones();

    // -------------在上面的初始化过程中，我们默认所有物理页都不可用，现在我们对可用的物理页，其对应区域要复位---------------
    // 同时，我们要对所有Zone结构体和Page结构体再次进行初始化
    for (int i = 0; i <= memory_management_struct.e820_length; i++)
    {
        if (memory_management_struct.e820[i].type == 1)
        {
            process_zone(&memory_management_struct.e820[i]);
        }
    }

    // 对于低1MB内存，其内存布局比较复杂，因此我们对于第1个物理页，要进行特殊的初始化,我们把它规划到第1个zone区域中
    memory_management_struct.pages_struct->zone_struct = memory_management_struct.zones_struct;

    memory_management_struct.pages_struct->PHY_address = 0UL;
    set_page_attribute(memory_management_struct.pages_struct, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
    memory_management_struct.pages_struct->reference_count = 1;
    memory_management_struct.pages_struct->create_time = 0;

    // 重新计算所有zone结构体占的字节数
    memory_management_struct.zones_length = (memory_management_struct.zones_size * sizeof(struct Zone) + sizeof(long) - 1) & (~(sizeof(long) - 1));

    ZONE_DMA_INDEX = 0;
    ZONE_NORMAL_INDEX = 0;
    ZONE_UNMAPED_INDEX = 0;

    for (int i = 0; i < memory_management_struct.zones_size; i++) // need rewrite in the future
    {
        struct Zone *z = memory_management_struct.zones_struct + i;
        printf("zone_start_address:%x,zone_end_address:%x,zone_length:%x,pages_group:%x,pages_length:%x\n", z->zone_start_address, z->zone_end_address, z->zone_length, z->pages_group, z->pages_length);
        // 如果zone的起始地址为1GB，记录zone的索引值，表示该区域没有映射过
        if (z->zone_start_address >= 0x100000000 && !ZONE_UNMAPED_INDEX)
            ZONE_UNMAPED_INDEX = i;
    }

    // 内存管理单元的结束地址
    memory_management_struct.end_of_struct = (uint64_t)((uint64_t)memory_management_struct.zones_struct + memory_management_struct.zones_length + sizeof(long) * 32) & (~(sizeof(long) - 1));

    printf("start_code:%x,end_code:x,end_data:%x,end_brk:%x,end_of_struct:%x\n", memory_management_struct.start_code, memory_management_struct.end_code, memory_management_struct.end_data, memory_management_struct.end_brk, memory_management_struct.end_of_struct);

    // 之前我们使用的地址都是虚拟地址，现在要对这些虚拟地址对应的物理页进行标记，标记它们使用过
    mark_used_pages();

    // 获取GDT的物理地址
    Global_CR3 = get_gdt();

    // 将页全局目录的前10项全部清0，这里不会引起异常，因为实际上我们的内核是映射到高半内存区域的，现在我们无法通过低内存地址访问低物理内存了
    // for (int i = 0; i < 10; i++)
    //     *(Phy_To_Virt(Global_CR3) + i) = 0UL;
    // 刷新页表
    flush_tlb();
}
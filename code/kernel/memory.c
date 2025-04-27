#include "include/memory.h"
#include "include/string.h"
#include "include/debug.h"
#include "include/print_kernel.h"
#include "include/bitmap.h"
#include "include/list.h"

struct Slab *kmalloc_create_slab(uint64_t size);

extern char _text;
extern char _etext;
extern char _edata;
extern char _end;

struct Global_Memory_Descriptor memory_management_struct = {{0}, 0};

int ZONE_DMA_INDEX = 0;
int ZONE_NORMAL_INDEX = 0;  // low 1GB RAM ,was mapped in pagetable
int ZONE_UNMAPED_INDEX = 0; // above 1GB RAM,unmapped in pagetable

uint64_t *Global_CR3 = NULL;

// 这个全局数据曾经没有被成功初始化，结果为0，原因是编译提取出来的纯二进制文件已经超过了50KB，但是写入到磁盘的时候，只写入了100扇区
// 所以内核程序被加载到内存中时，数据区的后面部分数据全为0      ~V~
struct Slab_cache kmalloc_cache_size[16] =
    {
        {32, 0, 0, NULL, NULL, NULL, NULL},
        {64, 0, 0, NULL, NULL, NULL, NULL},
        {128, 0, 0, NULL, NULL, NULL, NULL},
        {256, 0, 0, NULL, NULL, NULL, NULL},
        {512, 0, 0, NULL, NULL, NULL, NULL},
        {1024, 0, 0, NULL, NULL, NULL, NULL}, // 1KB
        {2048, 0, 0, NULL, NULL, NULL, NULL},
        {4096, 0, 0, NULL, NULL, NULL, NULL}, // 4KB
        {8192, 0, 0, NULL, NULL, NULL, NULL},
        {16384, 0, 0, NULL, NULL, NULL, NULL},
        {32768, 0, 0, NULL, NULL, NULL, NULL},
        {65536, 0, 0, NULL, NULL, NULL, NULL},  // 64KB
        {131072, 0, 0, NULL, NULL, NULL, NULL}, // 128KB
        {262144, 0, 0, NULL, NULL, NULL, NULL},
        {524288, 0, 0, NULL, NULL, NULL, NULL},
        {1048576, 0, 0, NULL, NULL, NULL, NULL}, // 1MB
};

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
    for (int i = 0; i < count; i++)
    {
        memory_management_struct.e820[i] = *p++;
        printf("e820 %d address: %x, length: %d tyep:%d\n", i, memory_management_struct.e820[i].address, memory_management_struct.e820[i].length, memory_management_struct.e820[i].type);
    }
    // 这里我们可以计算并打印出操作系统可用的2MB物理页的数量作为提示
    // 但是我这里不打印
}

// 初始化物理内存的描述位图
static void init_memory_bitmap()
{
    // 最大可用的物理内存
    uint64_t TotalMem = calculate_total_memory();

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
    printf("total memory: %d bytes = %d M\n", total_memory, total_memory / 1024 / 1024);
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
    printf("process zones_size %d\n", memory_management_struct.zones_size);
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
            printf("process zone %d\n", i);
            process_zone(&memory_management_struct.e820[i]);
        }
    }

    // 对于低1MB内存，其内存布局比较复杂，因此我们对于第1个物理页，要进行特殊的初始化,我们把它规划到第1个zone区域中
    memory_management_struct.pages_struct[0].zone_struct = memory_management_struct.zones_struct;

    memory_management_struct.pages_struct[0].PHY_address = 0UL;
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

    printf("start_code:%x, end_code:%x ,end_data:%x,end_brk:%x,end_of_struct:%x\n", memory_management_struct.start_code, memory_management_struct.end_code, memory_management_struct.end_data, memory_management_struct.end_brk, memory_management_struct.end_of_struct);

    // 之前我们使用的地址都是虚拟地址，现在要对这些虚拟地址对应的物理页进行标记，标记它们使用过
    mark_used_pages();
    for (int i = 0; i <= (Virt_To_Phy(memory_management_struct.end_of_struct) >> PAGE_2M_SHIFT); i++)
    {
        printf("%x ", memory_management_struct.bits_map[i]);
    }
    printf("\n");
    // 获取GDT的物理地址
    Global_CR3 = get_gdt();

    // 将页全局目录的前10项全部清0，这里不会引起异常，因为实际上我们的内核是映射到高半内存区域的，现在我们无法通过低内存地址访问低物理内存了
    // for (int i = 0; i < 10; i++)
    //     *(Phy_To_Virt(Global_CR3) + i) = 0UL;
    // 刷新页表
    flush_tlb();
}

//------------------------------------------------------------------------
//--------------------------以上是init_memory函数的实现--------------------
//------------------------------------------------------------------------



// 下面的函数是存在一些问题的，后面再进行优化,它假设了申请内存区域一定会成功，还有其它的细节问题
struct Page *alloc_pages(int zone_select, int number, uint64_t page_flags)
{
    uint64_t attribute = 0;
    // zone的开始索引
    int zone_start_index = 0;
    // zone的结束索引
    int zone_end_index = 0;

    if (number >= 64 || number <= 0)
    {
        printf("alloc_pages() ERROR: number is invalid\n");
        return NULL;
    }
    int free_index = 0;
    switch (zone_select)
    {
        // 直接映射区域,一个映射区域可能包含多个zone
        case ZONE_DMA:
            zone_start_index = 0;
            zone_end_index = ZONE_DMA_INDEX;
            attribute = PG_PTable_Maped;
            break;
        // 正常映射区域
        case ZONE_NORMAL:
            zone_start_index = ZONE_DMA_INDEX;
            zone_end_index = ZONE_NORMAL_INDEX;
            attribute = PG_PTable_Maped;
            break;
        // 未映射区域
        case ZONE_UNMAPED:
            zone_start_index = ZONE_UNMAPED_INDEX;
            zone_end_index = memory_management_struct.zones_size - 1;
            attribute = 0;
            break;
        default:
            printf("alloc_pages() ERROR: zone_select index is invalid\n");
            return NULL;
            break;
    }

    for (int i = zone_start_index; i <= zone_end_index; i++)
    {
        struct Zone *zone = memory_management_struct.zones_struct + i;
        uint64_t bit_start = zone->zone_start_address >> PAGE_2M_SHIFT;
        uint64_t bit_end = zone->zone_end_address >> PAGE_2M_SHIFT;
        uint64_t zone_bitmap_length = (bit_end - bit_start) / 8;
        struct bitmap map = {(uint64_t *)(bit_start), zone_bitmap_length};

        free_index = bitmap_scan(&map, number);

        if (free_index == -1)
        {
            continue;
        }
        for (int j = 0; j < number; j++)
        {
            struct Page *page = memory_management_struct.pages_struct + free_index + j;
            page_init(page, page_flags);
            *(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
            zone->page_using_count++;
            zone->page_free_count--;
        }
        break;
    }
    return memory_management_struct.pages_struct + free_index;
}

void free_pages(struct Page *page, int number)
{
    int i = 0;

    if (page == NULL)
    {
        printf("free_pages() ERROR: page is invalid\n");
        return;
    }

    if (number >= 64 || number <= 0)
    {
        printf("free_pages() ERROR: number is invalid\n");
        return;
    }

    for (i = 0; i < number; i++, page++)
    {
        *(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) &= ~(1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64);
        page->zone_struct->page_using_count--;
        page->zone_struct->page_free_count++;
        page->attribute = 0;
    }
}

//------------------------------------------------------------------------------------------------------
//--------------------以下几个函数是实现内存描述符初始化函数init_memory_slab的辅助函数-----------------------
//------------------------------------------------------------------------------------------------------

// 初始化slab_cache
static void init_slab_cache(struct Slab_cache *slab_cache, uint64_t block_size)
{
    // 把Slab_cache指向的Slab放在内存管理单元的末尾
    slab_cache->cache_pool = (struct Slab *)memory_management_struct.end_of_struct;

    // 更新内存管理单元的末尾
    memory_management_struct.end_of_struct = memory_management_struct.end_of_struct + sizeof(struct Slab) + sizeof(long) * 10;
    // ------------------------初始化slab------------------
    list_init(&(slab_cache->cache_pool->list));

    slab_cache->cache_pool->using_count = 0;
    slab_cache->cache_pool->free_count = PAGE_2M_SIZE / block_size;
    // 位图占用的字节数，向上取整8字节对齐
    slab_cache->cache_pool->color_length = (((PAGE_2M_SIZE / block_size) + 63) >> 6) << 3;
    slab_cache->cache_pool->color_count = PAGE_2M_SIZE / block_size;
    // 位图的放置位置
    slab_cache->cache_pool->color_map = (uint64_t *)memory_management_struct.end_of_struct;
    
    // 更新位图管理单元的结束位置，进行8字节对齐
    memory_management_struct.end_of_struct = (uint64_t)(memory_management_struct.end_of_struct + slab_cache->cache_pool->color_length + sizeof(long) * 10) & (~(sizeof(long) - 1));
        // 将位图中的位全部置1
        memset(slab_cache->cache_pool->color_map, 0xFF, slab_cache->cache_pool->color_length);
    // 将可用的内存块对应的位设为0
    // 这段代码有问题
    for (int i = 0; i < slab_cache->cache_pool->color_count; i++)
    {
        *(slab_cache->cache_pool->color_map + (i >> 6)) ^= 1UL << (i % 64);
    }

    slab_cache->total_free = slab_cache->cache_pool->free_count;
    slab_cache->total_using = 0;
}

// 标记使用过的物理页
static void mark_used_physical_pages(uint64_t start, uint64_t end, unsigned flags)
{
    uint64_t start_page = start >> PAGE_2M_SHIFT;
    uint64_t end_page = end >> PAGE_2M_SHIFT;

    for (uint64_t i = start_page; i <= end_page; ++i)
    {
        struct Page *page = &memory_management_struct.pages_struct[i];
        uint64_t *bits = memory_management_struct.bits_map + (i >> 6);
        // i & 63 相当于 i % 64
        *bits |= 1UL << (i & 63);

        page->zone_struct->page_using_count++;
        page->zone_struct->page_free_count--;
        page_init(page, flags);
    }
}

// 初始化内存块管理单元
uint64_t init_memory_slab()
{
    uint64_t start_address = memory_management_struct.end_of_struct;

    // 批量初始化Slab_cache
    for (int i = 0; i < 16; i++)
    {
        init_slab_cache(&kmalloc_cache_size[i], kmalloc_cache_size[i].size);
    }
    // ---------------------接下来对于所有slab占用的物理页进行置位标记------------------
    uint64_t end_address = Virt_To_Phy(memory_management_struct.end_of_struct);
    // start_address所在的物理页一定被标记过了，所以将start_address的地址向上取整2MB
    mark_used_physical_pages(PAGE_2M_UP_ALIGN(start_address), end_address, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
    // --------------------------为slab分配内存页-------------------
    for (int i = 0; i < 16; i++)
    {
        uint64_t *virtual = (uint64_t *)((memory_management_struct.end_of_struct + PAGE_2M_SIZE * i + PAGE_2M_SIZE - 1) & PAGE_2M_MASK);
        struct Page *page = Virt_To_2M_Page(virtual);

        *(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
        page->zone_struct->page_using_count++;
        page->zone_struct->page_free_count--;

        page_init(page, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);

        kmalloc_cache_size[i].cache_pool->page = page;
        kmalloc_cache_size[i].cache_pool->Vaddress = virtual;

    }

    return 1;
}

//------------------------------------------------------------------------------------------------------
//--------------------------------------init_memory_slab实现完毕----------------------------------------
//------------------------------------------------------------------------------------------------------




//------------------------------------------------------------------------------------------------------
//-----------------------------------以下函数用于辅助实现内存块的回收和释放---------------------------------
//------------------------------------------------------------------------------------------------------

// 从Slab_cache中查找一个可用的slab
static struct Slab *find_available_slab(struct Slab_cache *cache)
{
    struct Slab *slab = cache->cache_pool;
    do
    {
        if (slab->free_count == 0)
        {
            slab = container_of(list_next(&slab->list), struct Slab, list);
        }
        else
        {
            return slab;
        }
    } while (slab != cache->cache_pool);
    return NULL;
}

static void *allocate_from_slab(struct Slab *slab, struct Slab_cache *cache)
{
    struct bitmap map = {slab->color_map, slab->color_length};
    uint64_t idx = bitmap_scan(&map, 1);
    if (idx == -1)
        return NULL;

    bitmap_set(&map, idx, 1);
    slab->using_count++;
    slab->free_count--;
    cache->total_free--;
    cache->total_using++;

    return (void *)((char *)slab->Vaddress + (cache->size * idx));
}

void *kmalloc(uint64_t size, uint64_t gfp_flages)
{
    int i, j;
    struct Slab_cache *slab_cache = NULL;
    if (size > 1048576)
    {
        return NULL;
    }
    // 找到有对应大小的内存池
    for (i = 0; i < 16; i++)
        if (kmalloc_cache_size[i].size >= size)
            break;
    slab_cache = &kmalloc_cache_size[i];

    struct Slab *slab = find_available_slab(slab_cache);
    if (slab == NULL)
    {
        slab = kmalloc_create_slab(kmalloc_cache_size[i].size);
        if (slab == NULL)
        {
            return NULL;
        }
        slab_cache->total_free += slab->color_count;
        list_insert_before(&(slab_cache->cache_pool->list), &(slab->list));
    }
    return allocate_from_slab(slab, slab_cache);
}

// 小对象：在物理页内就地创建slab
static struct Slab *create_slab_in_page(uint64_t size)
{
    struct Page *page = alloc_pages(ZONE_NORMAL, 1, PG_Kernel);
    if (page == NULL)
    {
        printf("create_slab_in_page()->alloc_pages()=>page == NULL\n");
        return NULL;
    }
    void *vaddr = Phy_To_Virt(page->PHY_address);
    uint64_t bitmap_length = PAGE_2M_SIZE / size / 8;

    // 计算Slab和位图需要占据的空间大小，按8字节对齐
    uint64_t total_size = (sizeof(struct Slab) + bitmap_length + 8) & (~(sizeof(long) - 1));

    // 从物理页顶部分配空间
    struct Slab *slab = (struct Slab *)((char *)vaddr + PAGE_2M_SIZE - total_size);
    // 位图放置在slab后面
    slab->color_map = (uint64_t *)(slab + 1);
    // 位图长度向上取整8字节对齐
    slab->color_length = (bitmap_length + 8) & (~(sizeof(long) - 1));
    // 物理页中还可以使用的内存块数量
    slab->free_count = (PAGE_2M_SIZE - total_size) / size;
    slab->using_count = 0;
    slab->color_count = slab->free_count;
    slab->Vaddress = vaddr;
    slab->page = page;
    list_init(&slab->list);
    memset(slab->color_map, 0xff, slab->color_length);
    for (int i = 0; i < slab->color_count; i++)
        *(slab->color_map + (i >> 6)) ^= 1UL << i % 64;
    return slab;
}

static struct Slab *create_slab_out_of_page(uint64_t size)
{
    struct Page *page = alloc_pages(ZONE_NORMAL, 1, PG_Kernel);
    if (page == NULL)
    {
        printf("create_slab_out_of_page()->alloc_pages()=>page == NULL\n");
        return NULL;
    }
    struct Slab *slab = kmalloc(sizeof(struct Slab), 0);
    if (!slab)
    {
        return NULL;
    }

    slab->free_count = PAGE_2M_SIZE / size;
    slab->using_count = 0;
    slab->color_count = slab->free_count;
    // 位图长度向上取整8字节对齐
    slab->color_length = ((PAGE_2M_SIZE / size / 8) + 8) & (~(sizeof(long) - 1));
    slab->color_map = kmalloc(slab->color_length, 0);
    if (!slab->color_map)
    {
        kfree(slab);
        return NULL;
    }

    memset(slab->color_map, 0xff, slab->color_length);

    slab->Vaddress = Phy_To_Virt(page->PHY_address);
    slab->page = page;
    list_init(&slab->list);

    for (int i = 0; i < slab->color_count; i++)
        *(slab->color_map + (i >> 6)) ^= 1UL << i % 64;
    return slab;
}

// 对于512字节及以下的内存块，直接在申请的物理页面中放置Slab结构体及其位图
// 对于1024字节及以上的内存块，申请一个内存块来放置Slab结构体以及申请一个内存块来放置位图
struct Slab *kmalloc_create_slab(uint64_t size)
{
    struct Slab *slab = NULL;
    switch (size)
    {
    case 32:
    case 64:
    case 128:
    case 256:
    case 512:
        return create_slab_in_page(size);

    case 1024: // 1KB
    case 2048:
    case 4096: // 4KB
    case 8192:
    case 16384:
    case 32768:
    case 65536:
    case 131072: // 128KB
    case 262144:
    case 524288:
    case 1048576: // 1MB
        return create_slab_out_of_page(size);
    default:
        printf("kmalloc_create() ERROR: wrong size:%08d\n", size);
    }

    return NULL;
}

// 遍历每个Slab_cache,对于每个Slab_cache中的slab进行遍历，查看是否含有要释放的地址对应的内存块
// 这个函数的效率是比较偏低的，如果要提高效率，那么只能重新设计一下slba的算法，比如将slab结构体放在页开头
// 这样只要能找到内存块对应的物理页，就能快速找到相应的slab结构，能实现对内存块的快速回收
// 后续再进行优化吧
uint64_t kfree(void *address)
{
    int i;
    int index;
    struct Slab *slab = NULL;
    void *page_base_address = (void *)((uint64_t)address & PAGE_2M_MASK);

    for (i = 0; i < 16; i++)
    {
        slab = kmalloc_cache_size[i].cache_pool;
        do
        {
            if (slab->Vaddress == page_base_address)
            {
                index = (address - slab->Vaddress) / kmalloc_cache_size[i].size;

                *(slab->color_map + (index >> 6)) ^= 1UL << index % 64;

                slab->free_count++;
                slab->using_count--;

                kmalloc_cache_size[i].total_free++;
                kmalloc_cache_size[i].total_using--;

                if ((slab->using_count == 0) && (kmalloc_cache_size[i].total_free >= slab->color_count * 3 / 2) && (kmalloc_cache_size[i].cache_pool != slab))
                {
                    switch (kmalloc_cache_size[i].size)
                    {
                    case 32:
                    case 64:
                    case 128:
                    case 256:
                    case 512:
                        list_delete(&slab->list);
                        kmalloc_cache_size[i].total_free -= slab->color_count;

                        page_clean(slab->page);
                        free_pages(slab->page, 1);
                        break;

                    default:
                        list_delete(&slab->list);
                        kmalloc_cache_size[i].total_free -= slab->color_count;

                        kfree(slab->color_map);

                        page_clean(slab->page);
                        free_pages(slab->page, 1);
                        kfree(slab);
                        break;
                    }
                }

                return 1;
            }
            else
                slab = container_of(list_next(&slab->list), struct Slab, list);

        } while (slab != kmalloc_cache_size[i].cache_pool);
    }

    printf("kfree() ERROR: can`t free memory\n");

    return 0;
}





// 下面这个函数主要是将未映射过的物理内存页进行映射，但是ZONE_UNMAPED_INDEX代表的内存区域及其之后的内存区域都不映射
// 回想一下，线性地址转换到物理地址的过程，先是取线性地址的高9位并乘以8，得到在页全局目录中的偏移量（页全局目录项保存页上级目录的物理地址），进而可求得页上级目录的物理地址……
// 一直到从页表中获取到线性地址对应的物理页，然后在加上页中偏移量，就可以获得一个线性地址对应的物理地址
// 那么，将物理页映射为虚拟页的过程也就类似了，先将物理地址转换为虚拟地址，然后取虚拟地址的高9位并乘以8，得到页全局目录的偏移量（页上级目录的物理地址）
// 如果该页目录项为空，那么申请4KB的内存作为页上级目录，并将页上级目录的地址保存到页全局目录项中……
void pagetable_init()
{
    uint64_t i, j;
    uint64_t *tmp = NULL;

    Global_CR3 = get_gdt();

    // tmp指向页全局目录中的内核的页表项，该页表项保存内核的页上级目录地址
    tmp = (uint64_t *)(((uint64_t)Phy_To_Virt((uint64_t)Global_CR3 & (~0xfffUL))) + 8 * 256);

    //printf("1:%x\t\t\n", (uint64_t)tmp, *tmp);

    // 获取第1个页上级目录的基地址
    tmp = Phy_To_Virt(*tmp & (~0xfffUL));

    //printf("2:%x\t\t\n", (uint64_t)tmp, *tmp);

    // 获取第1个页中级目录的基地址
    tmp = Phy_To_Virt(*tmp & (~0xfffUL));

    //printf("3:%x\t\t\n", (uint64_t)tmp, *tmp);

    for (i = 0; i < memory_management_struct.zones_size; i++)
    {
        struct Zone *z = memory_management_struct.zones_struct + i;
        struct Page *p = z->pages_group;

        if (ZONE_UNMAPED_INDEX && i == ZONE_UNMAPED_INDEX)
            break;

        for (j = 0; j < z->pages_length; j++, p++);
        {
            // tmp指向页全局目录中的表项，表项和物理页的物理基地址有关
            tmp = (uint64_t *)(((uint64_t)Phy_To_Virt((uint64_t)Global_CR3 & (~0xfffUL))) + (((uint64_t)Phy_To_Virt(p->PHY_address) >> PAGE_GDT_SHIFT) & 0x1ff) * 8);

            // *tmp页全局目录表项保存页上级目录的线性地址，该项为空时表示不存在页上级目录，需要申请4KB空间作为页上级目录
            if (*tmp == 0)
            {
                uint64_t *virtual = kmalloc(PAGE_4K_SIZE, 0);
                set_mpl4t(tmp, mk_mpl4t(Virt_To_Phy(virtual), PAGE_KERNEL_GDT));
            }
            // tmp指向物理页对应的页上级目录的表项
            tmp = (uint64_t *)((uint64_t)Phy_To_Virt(*tmp & (~0xfffUL)) + (((uint64_t)Phy_To_Virt(p->PHY_address) >> PAGE_1G_SHIFT) & 0x1ff) * 8);

            // *tmp保存着页中级目录的地址，如果页中级目录不存在，则申请4KB页创建页中级目录
            if (*tmp == 0)
            {
                uint64_t *virtual = kmalloc(PAGE_4K_SIZE, 0);
                set_pdpt(tmp, mk_pdpt(Virt_To_Phy(virtual), PAGE_KERNEL_Dir));
            }
            // tmp指向页中级目录中对应2MB物理页的表项
            tmp = (uint64_t *)((uint64_t)Phy_To_Virt(*tmp & (~0xfffUL)) + (((uint64_t)Phy_To_Virt(p->PHY_address) >> PAGE_2M_SHIFT) & 0x1ff) * 8);
            // 让页中级目录中的表项指向对应的物理页
            set_pdt(tmp, mk_pdt(p->PHY_address, PAGE_KERNEL_Page));
        }
    }
    flush_tlb();
}
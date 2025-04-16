
#include "list.h"
#include "stdint.h"

#define KERNEL_START ((uint64_t)0xffff800000000000)

#define PAGE_GDT_SHIFT 39
#define PAGE_1G_SHIFT 30
#define PAGE_2M_SHIFT 21
#define PAGE_4K_SHIFT 12

#define PAGE_2M_SIZE (1UL << PAGE_2M_SHIFT)
#define PAGE_4K_SIZE (1UL << PAGE_4K_SHIFT)

#define PAGE_2M_MASK (~(PAGE_2M_SIZE - 1))
#define PAGE_4K_MASK (~(PAGE_4K_SIZE - 1))

#define PAGE_2M_UP_ALIGN(addr) (((uint64_t)(addr) + PAGE_2M_SIZE - 1) & PAGE_2M_MASK)
#define PAGE_2M_DOWN_ALIGN(addr) (((uint64_t)(addr)) & PAGE_2M_MASK)

#define PAGE_4K_ALIGN(addr) (((uint64_t)(addr) + PAGE_4K_SIZE - 1) & PAGE_4K_MASK)

#define Virt_To_Phy(addr) ((uint64_t)(addr) - KERNEL_START)
#define Phy_To_Virt(addr) ((uint64_t *)((uint64_t)(addr) + KERNEL_START))



// 页表项的低12位代表页的属性，因此这里模仿页表项对页属性的描述，方便创建页目录项、页表项的时候使用

//	bit 8	Global Page:1,global;0,part
#define PAGE_Global (uint64_t)0x0100

//	bit 7	Page Size:1,big page;0,small page;
#define PAGE_PS (uint64_t)0x0080

//	bit 6	Dirty:1,dirty;0,clean;
#define PAGE_Dirty (uint64_t)0x0040

//	bit 5	Accessed:1,visited;0,unvisited;
#define PAGE_Accessed (uint64_t)0x0020

//	bit 4	Page Level Cache Disable
#define PAGE_PCD (uint64_t)0x0010

//	bit 3	Page Level Write Through
#define PAGE_PWT (uint64_t)0x0008

//	bit 2	User Supervisor:1,user and supervisor;0,supervisor;
#define PAGE_U_S (uint64_t)0x0004

//	bit 1	Read Write:1,read and write;0,read;
#define PAGE_R_W (uint64_t)0x0002

//	bit 0	Present:1,present;0,no present;
#define PAGE_Present (uint64_t)0x0001

// 1,0
#define PAGE_KERNEL_GDT (PAGE_R_W | PAGE_Present)

// 1,0
#define PAGE_KERNEL_Dir (PAGE_R_W | PAGE_Present)

// 7,1,0
#define PAGE_KERNEL_Page (PAGE_PS | PAGE_R_W | PAGE_Present)

// 2,1,0
#define PAGE_USER_Dir (PAGE_U_S | PAGE_R_W | PAGE_Present)

// 7,2,1,0
#define PAGE_USER_Page (PAGE_PS | PAGE_U_S | PAGE_R_W | PAGE_Present)

struct E820
{
    uint64_t address;
    uint64_t length;
    unsigned type;
}__attribute__((packed));   // 为了防止编译器对结构体进行8字节对齐，指定packed属性让编译器使用紧凑格式

struct Global_Memory_Descriptor
{
    struct E820 e820[32];
    uint64_t e820_length;                  // ARDS结构体的个数 - 1

    uint64_t *bits_map;                    // 物理地址空间的页映射位图
    uint64_t bits_size;                    // 物理地址空间页的数量
    uint64_t bits_length;                  // 位图占据的字节数

    struct Page *pages_struct;                  // 指向全局页结构体数组的指针
    uint64_t pages_size;                   // 页结构体的数量
    uint64_t pages_length;                 // 所有页结构体的字节长度

    struct Zone *zones_struct;                  // 指向全局Zone结构体数组的指针
    uint64_t zones_size;                   // zone区域的数量
    uint64_t zones_length;                 // 所有zone结构体的字节长度

    uint64_t start_code, end_code, end_data, end_brk;      //内核程序的代码段起始地址，代码段结束地址，数据段的结束地址，整个程序的结束地址

    uint64_t end_of_struct;                // 内存页管理结构的结尾地址
};

// 该结构体用于描述一个页
struct Page
{
    struct Zone *zone_struct;           // 指向本页所属的区域结构体
    uint64_t PHY_address;          // 本页的物理地址
    uint64_t attribute;            // 页的属性

    uint64_t reference_count;      // 页的引用次数

    uint64_t create_time;                  // 页的创建时间
};

// 该结构体用于描述一个内存区域
struct Zone
{
    struct Page *pages_group;           // 本段内存区域的页结构体
    uint64_t pages_length;         // 本段内存区域的页数量

    uint64_t zone_start_address;   // 本段内存区域的起始地址
    uint64_t zone_end_address;     // 本段内存区域的结束地址
    uint64_t zone_length;          // 本段内存区域的长度
    uint64_t attribute;            // 本段内存区域的属性

    struct Global_Memory_Descriptor *GMD_struct; // 指向全局结构体Global_Memory_Descriptor

    uint64_t page_using_count;     // 本段内存区域已使用的页数量
    uint64_t page_free_count;      // 本段内存区域可使用的页数量

    uint64_t total_pages_link;     // 本段内存区域被引用的页数量
};

//
#define ZONE_DMA (1 << 0)

//
#define ZONE_NORMAL (1 << 1)

//
#define ZONE_UNMAPED (1 << 2)

// struct page attribute

//	mapped=1 or un-mapped=0
#define PG_PTable_Maped (1 << 0)

//	init-code=1 or normal-code/data=0
#define PG_Kernel_Init (1 << 1)

//	device=1 or memory=0
#define PG_Device (1 << 2)

//	kernel=1 or user=0
#define PG_Kernel (1 << 3)

//	shared=1 or single-use=0
#define PG_Shared (1 << 4)

#define flush_tlb_one(addr) \
    __asm__ __volatile__("invlpg	(%0)	\n\t" ::"r"(addr) : "memory")
/*

*/

#define flush_tlb()               \
    do                            \
    {                             \
        uint64_t tmpreg;     \
        __asm__ __volatile__(     \
            "movq	%%cr3,	%0	\n\t" \
            "movq	%0,	%%cr3	\n\t" \
            : "=r"(tmpreg)        \
            :                     \
            : "memory");          \
    } while (0)


// 使用inline会有找不到函数定义的报错
static uint64_t *get_gdt()
{
    uint64_t *tmp;
    __asm__ __volatile__(
        "movq	%%cr3,	%0	\n\t"
        : "=r"(tmp)
        :
        : "memory");
    return tmp;
}


#define SIZEOF_LONG_ALIGN(size) ((size + sizeof(long) - 1) & ~(sizeof(long) - 1))
#define SIZEOF_INT_ALIGN(size) ((size + sizeof(int) - 1) & ~(sizeof(int) - 1))

#define Virt_To_2M_Page(kaddr) (memory_management_struct.pages_struct + (Virt_To_Phy(kaddr) >> PAGE_2M_SHIFT))
#define Phy_to_2M_Page(kaddr) (memory_management_struct.pages_struct + ((uint64_t)(kaddr) >> PAGE_2M_SHIFT))

void init_memory();
struct Page *alloc_pages(int zone_select, int number, uint64_t page_flags);
void free_pages(struct Page *page, int number);
#include "include/cpu.h"
#include "include/print_kernel.h"
#include "include/stdint.h"
#include "include/memory.h"
#include "include/io.h"
#include "include/interrupt.h"

#define io_mfence() __asm__ __volatile__("mfence	\n\t" ::: "memory")
struct IOAPIC_map
{
	unsigned int physical_address;		  // I/O APIC的间接访问寄存器的基地址。
	unsigned char *virtual_index_address; // 索引寄存器的线性地址
	unsigned int *virtual_data_address;	  // 数据寄存器的线性地址
	unsigned int *virtual_EOI_address;	  // EOI寄存器的线性地址
} ioapic_map;

extern uint64_t *Global_CR3;

// 下面的函数首先是通过CPUID.01h检测了处理器是否支持APIC和x2APIC功能
// 然后配置IA32_APIC_BASE寄存器的位11和位10来打开x2APIC功能(硬启用x2APIC功能)
// 然后配置SVR寄存器的位8来启动Local APIC功能，配置位12禁止广播EOI消息(启用Local APIC)
// 配置（LVT）各个中断的工作模式
void Local_APIC_init()
{
	unsigned int x, y;
	unsigned int a, b, c, d;

	// 检测处理器是否支持APIC和x2APIC功能
	//  CPUID.01h:EDX[9]=1说明支持APIC功能，CPUID.01h:ECX[21]=1说明处理器支持x2APIC功能
	get_cpuid(1, 0, &a, &b, &c, &d);

	if ((1 << 9) & d)
	{
		printf("HW support APIC&xAPIC\t");
	}
	else
	{
		printf("HW NO support APIC&xAPIC\t");
	}

	if ((1 << 21) & c)
	{
		printf("HW support x2APIC\n");
	}
	else
	{
		printf("HW NO support x2APIC\n");
	}

	// 打开xAPIC和x2APIC功能
	// rdmsr指令的作用是读取msr寄存器（模型指定寄存器，也就是和CPU信息相关的寄存器），msr寄存器是一组寄存器，使用ECX指定要读取的MSR的地址
	// rdmsr指令的输出是64位，保存在EDX：EAX中，两个寄存器各自保存32位
	// IA32_APIC_BASE寄存器用于配置Local APIC寄存器的物理基地址和使能状态
	// 位8指示当前处理器为引导处理器，位11表示xAPIC全局使能标志位，位10表示是否开启x2APIC（必须在位11置位的时候才有效）
	// 从位12开始，后面的位表示APIC寄存器组的基地址，单位4KB。默认的基地址为FEE00000H。
	// bts指令的作用是将寄存器或内存中指定的位置1，同时将该位原来的值复制到CF中
	// wrmsr的作用是写入msr寄存器，作用与rdmsr相反，使用方法类似
	asm volatile("movq 	$0x1b,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 "bts	$10,	%%rax	\n\t"
						 "bts	$11,	%%rax	\n\t"
						 "wrmsr	\n\t"
						 "movq 	$0x1b,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 : "=a"(x), "=d"(y)
						 :
						 : "memory");

	printf("eax:0x%x,edx:0x%x\t", x, y);

	// 检测xAPIC和x2APIC功能是否打开
	if (x & 0xc00)
		printf("xAPIC & x2APIC enabled\n");

	// SVR寄存器的位8为APIC软件使能标志位，该位为1时表示打开Local APIC
	// 位12用于控制禁止广播EOI功能的开启与关闭
	// SVR寄存器的msr地址为0x80f
	asm volatile("movq 	$0x80f,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 "bts	$8,	%%rax	\n\t"
						 "bts	$12,%%rax\n\t"
						 "wrmsr	\n\t"
						 "movq 	$0x80f,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 : "=a"(x), "=d"(y)
						 :
						 : "memory");

	printf("eax:0x%x,edx:0x%x\t", x, y);

	if (x & 0x100)
		printf("SVR[8] enabled\n");
	if (x & 0x1000)
		printf("SVR[12] enabled\n");

	// 0x802是Local APIC ID寄存器的MSR地址
	// x2APICID是一个32位的值，表示Local APIC的ID值
	asm volatile("movq $0x802,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 : "=a"(x), "=d"(y)
						 :
						 : "memory");

	printf("eax:0x%x,edx:0x%x\tx2APIC ID:0x%x\n", x, y, x);

	// 0x803是Local APIC ID寄存器的MSR地址
	// 位0-7表示LoacalAPIC的版本ID，位16-23的值加1表示LVT的表项数，版本ID小于0x10时表示外部APIC芯片，大于16时表示内部APIC芯片
	// 位24表示是否支持禁止广播EOI功能
	asm volatile("movq $0x803,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 : "=a"(x), "=d"(y)
						 :
						 : "memory");

	printf("local APIC Version: 0x%x, Max LVT Entry: 0x%x, SVR(Suppress EOI Broadcast): 0x%x\t", x & 0xff, (x >> 16 & 0xff) + 1, x >> 24 & 0x1);

	if ((x & 0xff) < 0x10)
		printf("82489DX discrete APIC\n");

	else if (((x & 0xff) >= 0x10) && ((x & 0xff) <= 0x15))
		printf("Integrated APIC\n");

	// 设置LVT（Local Vector Table）中各个中断寄存器的工作模式
	// 一个中断寄存器占32位，位16表示中断屏蔽标志位，为1时表示屏蔽中断
	asm volatile("movq 	$0x82f,	%%rcx	\n\t" 			// CMCI
						 "wrmsr	\n\t"
						 "movq 	$0x832,	%%rcx	\n\t" 	// Timer
						 "wrmsr	\n\t"
						 "movq 	$0x833,	%%rcx	\n\t" 	// Thermal Monitor
						 "wrmsr	\n\t"
						 "movq 	$0x834,	%%rcx	\n\t" 	// Performance Counter
						 "wrmsr	\n\t"
						 "movq 	$0x835,	%%rcx	\n\t" 	// LINT0
						 "wrmsr	\n\t"
						 "movq 	$0x836,	%%rcx	\n\t" 	// LINT1
						 "wrmsr	\n\t"
						 "movq 	$0x837,	%%rcx	\n\t" 	// Error
						 "wrmsr	\n\t"
						 :
						 : "a"(0x10000), "d"(0x00)
						 : "memory");

	printf("Mask ALL LVT\n");

	// 读取TPR寄存器（任务优先权寄存器）的值，32位寄存器，低8位有效
	asm volatile("movq 	$0x808,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 : "=a"(x), "=d"(y)
						 :
						 : "memory");

	printf("Set LVT TPR:0x%x\t", x);

	// 读取PPR寄存器（处理器优先权寄存器）的值，32位寄存器，低8位有效
	asm volatile("movq 	$0x80a,	%%rcx	\n\t"
						 "rdmsr	\n\t"
						 : "=a"(x), "=d"(y)
						 :
						 : "memory");

	printf("Set LVT PPR:0x%x\n", x);
}

// I/O APIC包含大量的寄存器，但是只直接对外暴露三个寄存器，但这三个寄存器并不能通过物理地址直接访问，需要将它们映射到线性空间中才能访问
// I/O APIC中对外暴露的三个的寄存器的物理地址并不固定，一般默认情况下为0xFEC00000
// 间接索引寄存器、数据操作寄存器和EOI寄存器的物理地址分别为0xFECxy000，0xFECxy010，0xFECxy040，x和y分别表示一个16进制字符
// xy可通过OCI寄存器的低8位来配置，默认情况下xy为00
// 因此，默认情况下I/O APIC的寄存器的物理地址为0xFEC00000,将该地址映射到线性空间
// 下面的函数将I/O APIC中的寄存器的物理地址映射为线性地址
void IOAPIC_pagetable_remap()
{
	uint64_t *tmp;
	unsigned char *IOAPIC_addr = (unsigned char *)Phy_To_Virt(0xfec00000);

	ioapic_map.physical_address = 0xfec00000;
	ioapic_map.virtual_index_address = IOAPIC_addr;
	ioapic_map.virtual_data_address = (unsigned int *)(IOAPIC_addr + 0x10);
	ioapic_map.virtual_EOI_address = (unsigned int *)(IOAPIC_addr + 0x40);

	Global_CR3 = get_gdt();

	tmp = Phy_To_Virt(Global_CR3 + (((uint64_t)IOAPIC_addr >> PAGE_GDT_SHIFT) & 0x1ff));
	if (*tmp == 0)
	{
		uint64_t *virtual = kmalloc(PAGE_4K_SIZE, 0);
		set_mpl4t(tmp, mk_mpl4t(Virt_To_Phy(virtual), PAGE_KERNEL_GDT));
	}

	tmp = Phy_To_Virt((uint64_t *)(*tmp & (~0xfffUL)) + (((uint64_t)IOAPIC_addr >> PAGE_1G_SHIFT) & 0x1ff));
	if (*tmp == 0)
	{
		uint64_t *virtual = kmalloc(PAGE_4K_SIZE, 0);
		set_pdpt(tmp, mk_pdpt(Virt_To_Phy(virtual), PAGE_KERNEL_Dir));
	}

	tmp = Phy_To_Virt((uint64_t *)(*tmp & (~0xfffUL)) + (((uint64_t)IOAPIC_addr >> PAGE_2M_SHIFT) & 0x1ff));
	set_pdt(tmp, mk_pdt(ioapic_map.physical_address, PAGE_KERNEL_Page | PAGE_PWT | PAGE_PCD));

	flush_tlb();
}

// I/O APIC的数据寄存器位宽为32位，而RTE寄存器（可以理解为中断寄存器）的位宽为64位，因此需要经过两次读取才能获得RTE寄存器的值
uint64_t ioapic_rte_read(unsigned char index)
{
	uint64_t ret;

	// 先获取RTE的高32位
	*ioapic_map.virtual_index_address = index + 1;
	io_mfence();
	ret = *ioapic_map.virtual_data_address;
	ret <<= 32;
	io_mfence();
	// 再获取RTE的低32位
	*ioapic_map.virtual_index_address = index;
	io_mfence();
	ret |= *ioapic_map.virtual_data_address;
	io_mfence();

	return ret;
}

void ioapic_rte_write(unsigned char index, uint64_t value)
{
	*ioapic_map.virtual_index_address = index;
	io_mfence();
	*ioapic_map.virtual_data_address = value & 0xffffffff;
	value >>= 32;
	io_mfence();

	*ioapic_map.virtual_index_address = index + 1;
	io_mfence();
	*ioapic_map.virtual_data_address = value & 0xffffffff;
	io_mfence();
}

// I/O APIC中的其它寄存器是通过间接索引寄存器和数据寄存器来访问的
// 00h表示的是 I/O APIC ID寄存器（32位），24-27位表示ID，其余位为保留位
// 01h表示的是 I/O APIC 版本寄存器（32位），0-7位为APIC版本，16-23位的值加1表示可用RTE数，其余位保留
// 10h-11h……3Eh-3Fh表示中断寄存器，每个中断寄存器64位，位16为1时表示屏蔽该中断寄存器，低8位表示中断的向量号
void IOAPIC_init()
{
	// I/O APIC ID寄存器的24-27位写入版本号
	*ioapic_map.virtual_index_address = 0x00;
	io_mfence();
	*ioapic_map.virtual_data_address = 0x0f000000;
	io_mfence();
	printf("Get IOAPIC ID REG:%#010x,ID:%#010x\n", *ioapic_map.virtual_data_address, *ioapic_map.virtual_data_address >> 24 & 0xf);
	io_mfence();

	// 查询I/O APIC 版本寄存器中保存的版本号和RTE数量
	*ioapic_map.virtual_index_address = 0x01;
	io_mfence();
	printf("Get IOAPIC Version REG:0x%x,MAX redirection enties:%d\n", *ioapic_map.virtual_data_address, ((*ioapic_map.virtual_data_address >> 16) & 0xff) + 1);

	// 向0xcf8端口（32位）的最高位为1时（0x80000000）表示启用PCI配置空间
	// 0xcf8端口的0-1为保留，2-7位表示寄存器偏移量，8-10位表示功能号，11-15位表示设备号，16-23表示PCI总线号，31位​1表示启用配置空间访问（必须置1）
	// 构造PCI地址：0x80000000 | (bus << 16) | (device << 11) | (function << 8) | reg_offset & 0xFC
	// RCBA寄存器位于0号总线，31号设备，0号功能的F0偏移处
	// 不能直接读写RCBA寄存器，而是通过PCI配置地址端口（0xcf8）写入RCBA寄存器地址，通过PCI配置数据端口（0xcfc）读写RCBA寄存器
	outl(0xcf8, 0x80000000 | (0 << 16) | (31 << 11) | (0 << 8) | (0xF0 | 0xFC));
	uint32_t x = inl(0xcfc);
	// RCBA寄存器的14-31位表示RCBA物理地址
	x = x & 0xffffc000;
	printf("Get RCBA Address:0x%x\n", x);

	// OCI寄存器位于RCBA地址的31FEh地址处
	uint32_t *p;
	if (x > 0xfec00000 && x < 0xfee00000)
	{
		p = (unsigned int *)Phy_To_Virt(x + 0x31feUL);
	}

	// OCI寄存器的位8是I/O APIC的使能标志位
	x = (*p & 0xffffff00) | 0x100;
	io_mfence();
	*p = x;
	io_mfence();

	// 初始化24个中断寄存器,起始中断向量号为0x20，屏蔽24个中断
	for (int i = 0; i < 24; i++)
	{
		ioapic_rte_write(0x10 + i * 2, 0x10020 + i);
	}
	// 打开0x21号中断——键盘中断
	ioapic_rte_write(0x12, 0x21);
	printf("I/O APIC Redirection Table Entries Set Finished.\n");
}


void LAPIC_IOAPIC_init()
{
	int i;
	unsigned int x;
	unsigned int *p;

	IOAPIC_pagetable_remap();

	// 禁止使用8259A
	printf("MASK 8259A\n");
	outb(0x21, 0xff);
	outb(0xa1, 0xff);

	// IMCR（中断模式配置寄存器）用来控制使用类8259A还是APIC，它是一个间接访问寄存器
	// 通过向0x22端口写入IMCR的地址0x70，然后向0x23端口写入IMCR的值来配置IMCR
	// IMR为0x01时表示屏蔽8259A，也可以通过OCW1命令（8259A的中断屏蔽寄存器）来屏蔽8259A的所有中断,页可以通过屏蔽LVT的LINT0来实现
	// 这里显然我们重复屏蔽了8259A，但是没有关系
	outb(0x22, 0x70);
	outb(0x23, 0x01);

	Local_APIC_init();

	IOAPIC_init();

	// 在进入内核的时候，我们关闭了中断，所以此时不要忘记打开中断
	sti();
}
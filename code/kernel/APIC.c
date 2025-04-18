#include "include/cpu.h"
#include "include/print_kernel.h"

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
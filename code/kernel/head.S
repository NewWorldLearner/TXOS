
.section .text

.globl _start

_start:

	mov	$0x10,	%ax
	mov	%ax,	%ds
	mov	%ax,	%es
	mov	%ax,	%fs
	mov	%ax,	%ss
	mov	$0x7E00,	%esp

//=======	load GDTR

	lgdt	GDT_POINTER(%rip)

//=======	load	IDTR

	lidt	IDT_POINTER(%rip)


	mov	$0x10,	%ax		
	mov	%ax,	%ds
	mov	%ax,	%es
	mov	%ax,	%fs
	mov	%ax,	%gs
	mov	%ax,	%ss

	movq	$0x7E00,	%rsp

//=======	load	cr3

	movq	$0x101000,	%rax
	movq	%rax,		%cr3
	movq	switch_seg(%rip),	%rax
	pushq	$0x08
	pushq	%rax
	retfq

//=======	64-bit mode code

switch_seg:
	.quad	entry64

entry64:
	movq	$0x10,	%rax
	movq	%rax,	%ds
	movq	%rax,	%es
	movq	%rax,	%gs
	movq	%rax,	%ss
	movq	_stack_start(%rip),	%rsp		// 内核进程的栈底

setup_TSS64:
	leaq	TSS64_Table(%rip),	%rdx
	xorq	%rax,	%rax
	xorq	%rcx,	%rcx
	movq	$0x89,	%rax					// 设置类型字段为0x89（64位TSS，P=1, DPL=0, Type=9）
	shlq	$40,	%rax					// RAX保存TSS描述符的低64位，左移40位，将类型字段放到TSS描述符的40位

	movl	%edx,	%ecx					// 取TSS地址的低32位到ECX
	shrl	$24,	%ecx					// 取TSS基地址的31-24位
	shlq	$56,	%rcx					// 左移56位，RCX的高8位保存TSS基地址的31-24位
	addq	%rcx,	%rax					// 合并基地址31-24位到RAX

	xorq	%rcx,	%rcx
	movl	%edx,	%ecx					// 重新加载TSS地址的低32位到ECX
	andl	$0xffffff,	%ecx				// 保留基地址的低24位（基地址的0-23位）
	shlq	$16,	%rcx					// 左移16位，TSS描述符的低64位的16-39位保存TSS基地址的0-23位
	addq	%rcx,	%rax					// 合并到RAX

	addq	$103,	%rax					// TSS描述符的0-15，48-51位为段限长，设为103

	leaq	GDT_Table(%rip),	%rdi		// 加载GDT基地址到RDI
	movq	%rax,	80(%rdi)				// 写入TSS描述符的低64位到GDT偏移量为80的地方
	shrq	$32,	%rdx					// 取TSS地址的高32位
	movq	%rdx,	88(%rdi)				// 写入TSS描述符的高64位到GDT偏移量为88的地方

	mov	$0x50,	%ax							// TSS描述符对应的选择子为8*10=0x50
	ltr	%ax									// 加载TSS

	movq	go_to_kernel(%rip),	%rax		/* movq address */
	pushq	$0x08
	pushq	%rax
	lretq

go_to_kernel:
	.quad	Start_Kernel

//=======	init page
.align 8

.org	0x1000				//将页全局目录的地址设置到0x101000处

__PML4E:

	.quad	0x102007
	.fill	255,8,0
	.quad	0x102007
	.fill	254,8,0
	.quad   0x101003
.org	0x2000				//页上级目录的地址设置到0x102000处

__PDPTE:
	
	.quad	0x103003
	.fill	511,8,0

.org	0x3000

__PDE:						//页中级目录的地址设置到0x103000处

// 将物理地址0开始的48MB内存映射到线性地址0x00和0xffff800000000000处

	.quad	0x000083	
	.quad	0x200083
	.quad	0x400083
	.quad	0x600083
	.quad	0x800083		/* 0x800083 */
	.quad	0xa00083
	.quad	0xc00083
	.quad	0xe00083
	.quad	0x1000083
	.quad	0x1200083
	.quad	0x1400083
	.quad	0x1600083
	.quad	0x1800083
	.quad	0x1a00083
	.quad	0x1c00083
	.quad	0x1e00083
	.quad	0x2000083
	.quad	0x2200083
	.quad	0x2400083
	.quad	0x2600083
	.quad	0x2800083
	.quad	0x2a00083
	.quad	0x2c00083
	.quad	0x2e00083

	.quad	0xe0000083		/*0x 3000000*/
	.quad	0xe0200083
	.quad	0xe0400083
	.quad	0xe0600083
	.quad	0xe0800083
	.quad	0xe0a00083
	.quad	0xe0c00083
	.quad	0xe0e00083
	.fill	480,8,0

//=======	GDT_Table

.section .data

.globl GDT_Table

GDT_Table:
	.quad	0x0000000000000000			/*0	NULL descriptor		       	00*/
	.quad	0x0020980000000000			/*1	KERNEL	Code	64-bit	Segment	08*/
	.quad	0x0000920000000000			/*2	KERNEL	Data	64-bit	Segment	10*/
	.quad	0x0020f80000000000			/*3	USER	Code	64-bit	Segment	18*/
	.quad	0x0000f20000000000			/*4	USER	Data	64-bit	Segment	20*/
	.quad	0x00cf9a000000ffff			/*5	KERNEL	Code	32-bit	Segment	28*/
	.quad	0x00cf92000000ffff			/*6	KERNEL	Data	32-bit	Segment	30*/
	.fill	10,8,0					    /*8 ~ 9	TSS (jmp one segment <7>) in long-mode 128-bit 40 */
GDT_END:

GDT_POINTER:
GDT_LIMIT:	.word	GDT_END - GDT_Table - 1
GDT_BASE:	.quad	GDT_Table

//=======	IDT_Table

.globl IDT_Table

IDT_Table:
	.fill  512,8,0
IDT_END:

IDT_POINTER:
IDT_LIMIT:	.word	IDT_END - IDT_Table - 1
IDT_BASE:	.quad	IDT_Table

//=======	TSS64_Table

.globl	TSS64_Table

TSS64_Table:
	.fill  13,8,0
TSS64_END:

TSS64_POINTER:
TSS64_LIMIT:	.word	TSS64_END - TSS64_Table - 1
TSS64_BASE:	.quad	TSS64_Table

.globl _stack_start

_stack_start:
	.quad	kernel_task_union + 32768


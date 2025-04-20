#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include "stdint.h"

typedef void* intr_handler;

//--------------   IDT描述符属性  ------------

#define INTERRUPT 0xE   // 中断门类型标志
#define TRAP 0xF        // 陷阱门类型标志

#define NO_IST 0

// 异常类型有3种，错误，陷阱，终止，加上中断，共4种
// 对于错误和终止，都使用中断门来表示，陷阱门的属性我们先定义相应的宏，但是对于所有的中断向量，我们都统一使用中断门（注意到0-31号异常不受IF标志位的影响）
// 中断门和陷阱门的区别就是，中断门会自动关闭IF标志位，而陷阱门则不会，而我们可以在中断入口函数中手动打开和关闭IF标志位，因此这两种门应该没啥区别
// 下面一个字节代表的属性为P(位7)，DPL(位5-6)，位4固定为0，TYPE(位0-3)，TYPE=0xE代表中断门，TYPE=0xF代表陷阱门
#define IDT_DESC_ATTR_FAULT 0x8E    // 错误门，P=1,DPL=0,TYPE=0xF
#define IDT_DESC_ATTR_TRAP 0x8F     // 陷阱门，P=1,DPL=0,TYPE=0xE
#define IDT_DESC_ATTR_INTERRUPT 0x8E // 外部中断，P=1,DPL=0,TYPE=0xF
#define IDT_DESC_ATTR_SYSTEM 0xEE    // 系统调用，P=1,DPL=3,TYPE=0xF




// HardWare interrupt type

struct pt_regs
{
    uint64_t es;
    uint64_t ds;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    // 发生中断时，会自动压入EFLAGS，CS，IP，如果有特权级转移，还会压入原来的SS和ESP，对于异常还有错误码会自动压入
    uint64_t errcode;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

// 中断寄存器的操作接口集合
typedef struct hw_int_type
{
    void (*enable)(uint64_t irq);  // 使能中断操作接口
    void (*disable)(uint64_t irq); // 禁止中断操作接口

    uint64_t (*install)(uint64_t irq, void *arg); // 安装中断操作接口
    void (*uninstall)(uint64_t irq);              // 卸载中断操作接口

    void (*ack)(uint64_t irq); // 应答中断操作接口
} hw_int_controller;

// interrupt request descriptor Type
typedef struct
{
    hw_int_controller *controller;

    char *irq_name;                                                         // 中断名字
    uint64_t parameter;                                                     // 中断处理函数的参数
    void (*handler)(uint64_t nr, uint64_t parameter, struct pt_regs *regs); // 中断处理函数
    uint64_t flags;                                                         // 自定义标志位
} irq_desc_T;

inline static void sti()
{
    // cc 表示EFLAGS寄存器被修改
    asm volatile ("sti \n\t": : : "cc");
}

inline static void cli()
{
    // cc 表示EFLAGS寄存器被修改
    asm volatile("cli \n\t" : : : "cc");
}

void idt_init();

int register_irq(uint64_t irq,
                 void *arg,                                                              // 中断寄存器操作接口（安装函数）的参数
                 void (*handler)(uint64_t nr, uint64_t parameter, struct pt_regs *regs), // 中断处理函数
                 uint64_t parameter,                                                     // 中断处理函数的参数
                 hw_int_controller *controller,                                          // 中断寄存器的操作接口
                 char *irq_name);                                                        // 中断向量名字

int unregister_irq(uint64_t irq);


#endif
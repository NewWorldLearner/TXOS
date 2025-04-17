#ifndef _INTERRUPT_H
#define _INTERRUPT_H

typedef void* intr_handler;

//--------------   IDT描述符属性  ------------

#define INTERRUPT 0xE   // 中断门类型标志
#define TRAP 0xF        // 陷阱门类型标志

#define NO_IST 0

// 异常类型有3种，错误，陷阱，终止，加上中断，共4种
// 对于错误和终止，都使用中断门来表示，陷阱门的属性我们先定义相应的宏，但是对于所有的中断向量，我们都统一使用中断门
// 中断门和陷阱门的区别就是，中断门会自动关闭IF标志位，而陷阱门则不会，而我们可以在中断入口函数中手动打开和关闭IF标志位，因此这两种门应该没啥区别
// 下面一个字节代表的属性为P(位7)，DPL(位5-6)，位4固定为0，TYPE(位0-3)，TYPE=0xE代表中断门，TYPE=0xF代表陷阱门
#define IDT_DESC_ATTR_FAULT 0x8E    // 错误门，P=1,DPL=0,TYPE=0xF
#define IDT_DESC_ATTR_TRAP 0x8F     // 陷阱门，P=1,DPL=0,TYPE=0xE
#define IDT_DESC_ATTR_INTERRUPT 0x8E // 外部中断，P=1,DPL=0,TYPE=0xF
#define IDT_DESC_ATTR_SYSTEM 0xEE    // 系统调用，P=1,DPL=3,TYPE=0xF

void idt_init();

#endif
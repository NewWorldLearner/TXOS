#include "include/interrupt.h"
#include "include/print_kernel.h"
#include "include/io.h"

#define NULL (void*)0

#define PIC_M_CTRL 0x20 // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21 // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0 // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1 // 从片的数据端口是0xa1

#define IDT_DESC_CNT 56

// 128位中断门描述符结构体
struct gate_desc
{
    uint16_t func_offset_low_word; // 代码段的0-15位偏移量
    uint16_t selector;             // 段选择子
    uint8_t ist;                   // IST位于0-2位，其余为固定为0
    uint8_t attribute;
    uint16_t func_offset_high_word; // 代码段的16-31位偏移量
    uint32_t func_offset_32_63;     // 代码段的32-63位偏移量
    uint32_t nonsense;              // 这个字段无意义
};

extern intr_handler intr_entry_table[IDT_DESC_CNT]; // 声明引用定义在kernel.S中的中断处理函数入口数组

extern struct gate_desc IDT_Table[256];

intr_handler idt_func_table[256];

// 24个外部中断
irq_desc_T interrupt_desc[24] = {0};


//////////////////////////////////////////////////////////////////////////////
//                    下面是中断向量的处理函数
/////////////////////////////////////////////////////////////////////////////

// 0
void do_div_zero_error(struct pt_regs *regs, uint64_t irq)
{
    printf("div zero error\n");
    while (1)
        ;
}

// 1
void do_debug(struct pt_regs *regs, uint64_t irq)
{
    printf("debug error\n");
    while (1)
        ;
}

// 2
void do_nmi(struct pt_regs *regs, uint64_t irq)
{
    printf("nmi error\n");
    while (1)
        ;
}

// 3
void do_int3(struct pt_regs *regs, uint64_t irq)
{
    printf("int3 error\n");
    while (1)
        ;
}

// 4
void do_overflow(struct pt_regs *regs, uint64_t irq)
{
    printf("overflow error\n");
    while (1)
        ;
}

// 5
void do_bounds(struct pt_regs *regs, uint64_t irq)
{
    printf("bounds error\n");
    while (1)
        ;
}

// 6
void do_undefined_code(struct pt_regs *regs, uint64_t irq)
{
    printf("undefined_code error!  rip:%x     rsp: %x   error:%x\n", regs->rip,regs->rsp, regs->errcode);
    while (1)
        ;
}

// 7
void do_device_not_available(struct pt_regs *regs, uint64_t irq)
{
    printf("device_not_available error\n");
    while (1)
        ;
}

// 8
void do_double_fault(struct pt_regs *regs, uint64_t irq)
{
    printf("double error\n");
    while (1)
        ;
}

// 9
void do_coprocessor_segment_overrun(struct pt_regs *regs, uint64_t irq)
{
    printf("coprocessor_segment_overrun\n");
    while (1)
        ;
}

// 10
void do_invalid_TSS(struct pt_regs *regs, uint64_t irq)
{
    printf("invalid TSS error\n");
    while (1)
        ;
}

// 11
void do_segment_not_present(struct pt_regs *regs, uint64_t irq)
{
    printf("segment_not_present error\n");
    while (1)
        ;
}

// 12
void do_stack_segment_fault(struct pt_regs *regs, uint64_t irq)
{
    printf("stack_segment_fault\n");
    while (1)
        ;
}

// 13
void do_general_protection(struct pt_regs *regs, uint64_t irq)
{
    printf("do_general_protection, error code 0x%x\n", regs->errcode);
    while (1)
        ;
}

// 14
void do_page_fault(struct pt_regs *regs, uint64_t irq)
{
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    printf("do_page_fault  cr2:0x%x\n",cr2);
    while (1)
        ;
}

// 15号异常保留不用

// 16
void do_x87_FPU_error(struct pt_regs *regs, uint64_t irq)
{
    printf("do_x87_FPU_error\n");
    while (1)
        ;
}

// 17
void do_alignment_check(struct pt_regs *regs, uint64_t irq)
{
    printf("do_alignment_check\n");
    while (1)
        ;
}

// 18
void do_machine_check(struct pt_regs *regs, uint64_t irq)
{
    printf("do_machine_check\n");
    while (1)
        ;
}

// 19
void do_SIMD_exception(struct pt_regs *regs, uint64_t irq)
{
    printf("do_SIMD_exception\n");
    while (1)
        ;
}

// 20
void do_virtualization_exception(struct pt_regs *regs, uint64_t irq)
{
    printf("do_virtualization_exception\n");
    while (1)
        ;
}

// 21-31号是intel保留未使用的，32-255用户自定义使用

// 通用的中断处理函数,一般用在异常出现时的处理
static void general_intr_handler(struct pt_regs *regs, uint64_t irq)
{
    // 后面再具体设计中断处理函数的形式吧
    printf("interrupt to do: interrupt number %d\n",irq);
    // 发送中断结束命令
    outb(0x20, 0x20);
    outb(0xa0, 0x20);
}

// 32-55号为外部中断，统一使用do_IRQ处理，在该函数中再根据中断向量号跳转到具体的中断处理函数
void do_IRQ(struct pt_regs *regs, uint64_t nr)
{
    uint8_t x;
    irq_desc_T *irq = &interrupt_desc[nr - 32];

    // 调用具体的中断处理函数
    if (irq->handler != NULL)
    {
        irq->handler(nr, irq->parameter, regs);
    }

    // 中断应答，比如发送EOI
    if (irq->controller != NULL && irq->controller->ack != NULL)
    {
        irq->controller->ack(nr);
    }
}


////////////////////////////////////////////////////////////////
//               以上是中断向量的处理函数
////////////////////////////////////////////////////////////////

// 创建中断门描述符
static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t ist, uint8_t attr, intr_handler function)
{
    p_gdesc->func_offset_low_word = (uint64_t)function & 0x0000FFFF;
    p_gdesc->selector = 0x08; // 0x08表示内核代码段选择子
    p_gdesc->ist = ist;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint64_t)function >> 16) & 0xFFFF;
    p_gdesc->func_offset_32_63 = ((uint64_t)function >> 32) & 0xFFFFFFFF;
}

// 初始化中断描述符表
static void idt_table_init(void)
{
    int i;
    // 陷阱门我们先不关注，直接把它当作中断门，后续再看看要不要把陷阱门改成中断门
    for (i = 0; i < IDT_DESC_CNT; i++)
    {
        make_idt_desc(&IDT_Table[i], 0, IDT_DESC_ATTR_TRAP, intr_entry_table[i]);
    }
    printf("idt_desc_init done\n");
}

// 为中断处理函数表赋值
static void idt_func_table_init()
{
    for (int i = 0; i < IDT_DESC_CNT; i++)
    {
        idt_func_table[i] = general_intr_handler; // 默认为general_intr_handler。
    }

    idt_func_table[0] = do_div_zero_error;
    idt_func_table[1] = do_debug;
    idt_func_table[2] = do_nmi;
    idt_func_table[3] = do_int3;
    idt_func_table[4] = do_overflow;
    idt_func_table[5] = do_bounds;
    idt_func_table[6] = do_undefined_code;
    idt_func_table[7] = do_device_not_available;
    idt_func_table[8] = do_double_fault;
    idt_func_table[9] = do_coprocessor_segment_overrun;
    idt_func_table[10] = do_invalid_TSS;
    idt_func_table[11] = do_segment_not_present;
    idt_func_table[12] = do_stack_segment_fault;
    idt_func_table[13] = do_general_protection;
    idt_func_table[14] = do_page_fault;
    // 15号异常保留不用
    idt_func_table[16] = do_x87_FPU_error;
    idt_func_table[17] = do_alignment_check;
    idt_func_table[18] = do_machine_check;
    idt_func_table[19] = do_SIMD_exception;
    idt_func_table[20] = do_virtualization_exception;
    // 21-31号中断是intel保留未使用的
    // 32-55作为外部中断，中断处理函数统一为do_IRQ，然后在该函数中根据中断向量号来调用具体的中断处理函数
    for (int i = 32; i <56; i++)
    {
        idt_func_table[i] = do_IRQ;
    }
}

// 完成有关中断的所有初始化工作
void idt_init()
{
    printf("idt_init start\n");
    idt_table_init();
    idt_func_table_init();
    printf("idt_init done\n");
}

// 注册外部中断处理函数
// 其实就是给中断控制结构体irq_desc_T赋值，然后调用install函数和enable函数
int register_irq(uint64_t irq,
                 void *arg,
                 void (*handler)(uint64_t irq, uint64_t parameter, struct pt_regs *regs),
                 uint64_t parameter,
                 hw_int_controller *controller,
                 char *irq_name)
{
    irq_desc_T *p = &interrupt_desc[irq - 32];

    p->controller = controller;
    p->irq_name = irq_name;
    p->parameter = parameter;
    p->flags = 0;
    p->handler = handler;

    p->controller->install(irq, arg);
    p->controller->enable(irq);

    return 1;
}

// 卸载外部中断处理函数
int unregister_irq(uint64_t irq)
{
    irq_desc_T *p = &interrupt_desc[irq - 32];

    p->controller->disable(irq);
    p->controller->uninstall(irq);

    p->controller = NULL;
    p->irq_name = NULL;
    p->parameter = 0;
    p->flags = 0;
    p->handler = NULL;

    return 1;
}
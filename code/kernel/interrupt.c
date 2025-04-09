#include <stdint.h>
#include "interrupt.h"
#include "print_kernel.h"

#define IDT_DESC_CNT 30

// 128位中断门描述符结构体
struct gate_desc
{
    uint16_t func_offset_low_word;      // 代码段的0-15位偏移量
    uint16_t selector;                  // 段选择子
    uint8_t ist;                     // IST位于0-2位，其余为固定为0
    uint8_t attribute;
    uint16_t func_offset_high_word;     // 代码段的16-31位偏移量
    uint32_t func_offset_32_63;         // 代码段的32-63位偏移量
    uint32_t nonsense;                  // 这个字段无意义
};

// 静态函数声明,非必须
static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t ist, uint8_t attr, intr_handler function);

extern intr_handler intr_entry_table[IDT_DESC_CNT]; // 声明引用定义在kernel.S中的中断处理函数入口数组

extern struct gate_desc IDT_Table[256];

intr_handler idt_func_table[48];

char *intr_name[IDT_DESC_CNT];

/* 创建中断门描述符 */
static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t ist, uint8_t attr, intr_handler function)
{
    p_gdesc->func_offset_low_word = (uint64_t)function & 0x0000FFFF;
    p_gdesc->selector = 0x08;       // 0x08表示内核代码段选择子
    p_gdesc->ist = ist;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint64_t)function >> 16) & 0xFFFF;
    p_gdesc->func_offset_32_63 = ((uint64_t)function >> 32) & 0xFFFFFFFF;
}

/*初始化中断描述符表*/
static void idt_desc_init(void)
{
    int i;
    // 陷阱门我们先不关注，直接把它当作中断门，后续再看看要不要把陷阱门改成中断门
    for (i = 0; i < IDT_DESC_CNT; i++)
    {
        make_idt_desc(&IDT_Table[i], 0, IDT_DESC_ATTR_SYSTEM, intr_entry_table[i]);
    }
    make_idt_desc(&IDT_Table[0], 0, IDT_DESC_ATTR_TRAP, intr_entry_table[0]);
    print_str("idt_desc_init done\n");
}

/* 通用的中断处理函数,一般用在异常出现时的处理 */
static void general_intr_handler(uint8_t vec_nr)
{
    if (vec_nr == 0x27 || vec_nr == 0x2f)
    {           // 0x2f是从片8259A上的最后一个irq引脚，保留
        return; // IRQ7和IRQ15会产生伪中断(spurious interrupt),无须处理。
    }
    print_str("int vector: 0x\n");
    // put_int(vec_nr);
    // put_char('\n');
}

/* 完成一般中断处理函数注册及异常名称注册 */
static void exception_init(void)
{ // 完成一般中断处理函数注册及异常名称注册
    int i;
    for (i = 0; i < IDT_DESC_CNT; i++)
    {

        /* idt_table数组中的函数是在进入中断后根据中断向量号调用的,
         * 见kernel/kernel.S的call [idt_table + %1*4] */
        idt_func_table[i] = general_intr_handler; // 默认为general_intr_handler。
                                             // 以后会由register_handler来注册具体处理函数。
        intr_name[i] = "unknown";            // 先统一赋值为unknown
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第15项是intel保留项，未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}




void do_div_zero_error(uint8_t vec_no)
{
    print_str("div zero error\n");
    while(1);
}

void do_debug(uint8_t vec_no)
{
    print_str("debug error\n");
    while (1)
        ;
}

void do_nmi(uint8_t vec_no)
{
    print_str("nmi error\n");
    while (1)
        ;
}

void do_int3(uint8_t vec_no)
{
    print_str("int3 error\n");
    while (1)
        ;
}

void do_overflow(uint8_t vec_no)
{
    print_str("overflow error\n");
    while (1)
        ;
}

void do_bounds(uint8_t vec_no)
{
    print_str("bounds error\n");
    while (1)
        ;
}

void do_undefined_code(uint8_t vec_no)
{
    print_str("undefined_code error\n");
    while (1)
        ;
}

void do_device_not_available(unsigned long rsp, unsigned long error_code)
{
    print_str("device_not_available error\n");
    while (1)
        ;
}

/*

*/

void do_double_fault(unsigned long rsp, unsigned long error_code)
{
    print_str("double error\n");
    while (1)
        ;
}

/*

*/

void do_coprocessor_segment_overrun(unsigned long rsp, unsigned long error_code)
{
    print_str("coprocessor_segment_overrun\n");
    while (1)
        ;
}

/*

*/

void do_invalid_TSS(unsigned long rsp, unsigned long error_code)
{
    print_str("invalid TSS error\n");
    while (1)
        ;
}

/*

*/

void do_segment_not_present(unsigned long rsp, unsigned long error_code)
{
    print_str("segment_not_present error\n");
    while (1)
        ;
}

/*

*/

void do_stack_segment_fault(unsigned long rsp, unsigned long error_code)
{
    print_str("stack_segment_fault\n");
    while (1)
        ;
}

/*

*/

void do_general_protection(unsigned long rsp, unsigned long error_code)
{
    print_str("do_general_protection\n");
    while (1)
        ;
}

/*

*/

void do_page_fault(unsigned long rsp, unsigned long error_code)
{
    print_str("do_page_fault\n");
    while (1)
        ;
}

/*

*/

void do_x87_FPU_error(unsigned long rsp, unsigned long error_code)
{
    print_str("do_x87_FPU_error\n");
    while (1)
        ;
}

/* 在中断处理程序数组第vector_no个元素中注册安装中断处理程序function */
void register_handler(uint8_t vector_no, intr_handler function)
{
    /* idt_table数组中的函数是在进入中断后根据中断向量号调用的,
     * 见kernel/kernel.S的call [idt_table + %1*4] */
    idt_func_table[0]=do_div_zero_error;
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

}

/*完成有关中断的所有初始化工作*/
void idt_init()
{
    print_str("idt_init start\n");
    idt_desc_init(); // 初始化中断描述符表
    exception_init();
    print_str("idt_init done\n");

    int i = 0;
    for (i = 0; i < 48; i++)
    {
        register_handler(0, general_intr_handler);
    }
}
typedef void* intr_handler;

void idt_init();

//--------------   IDT描述符属性  ------------

#define INTERRUPT 0xF   // 中断门类型标志
#define TRAP 0xE        // 陷阱门类型标志

#define NO_IST 0

// 组合成4种门类型，错误，陷阱，终止，中断（系统调用）
// 但是这里我们只区分两种门，陷阱门和中断门
// 下面一个字节代表的属性为P(位7)，DPL(位5-6)，位4固定为0，TYPE(位0-3)，TYPE=0xF代表中断门，TYPE=0xE代表陷阱门
#define IDT_DESC_ATTR_FAULT 0xEF    // 错误门，P=1,DPL=0,TYPE=0xF
#define IDT_DESC_ATTR_TRAP 0xEE     // 陷阱门，P=1,DPL=0,TYPE=0xE
#define IDT_DESC_ATTR_INTERRUPT 0xEF // 外部中断，P=1,DPL=0,TYPE=0xF
#define IDT_DESC_ATTR_SYSTEM 0xEF    // 系统调用，P=1,DPL=0,TYPE=0xF

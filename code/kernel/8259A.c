#include "include/io.h"
#include "include/8259A.h"
#include "include/print_kernel.h"
#include "include/interrupt.h"

// 初始化可编程中断控制器8259A
void pic_8259A_init()
{
    // 初始化主片
    outb(PIC_M_CTRL, 0x11); // ICW1: 边沿触发,级联8259, 需要ICW4.
    outb(PIC_M_DATA, 0x20); // ICW2: 起始中断向量号为0x20,也就是IR[0-7] 为 0x20 ~ 0x27.
    outb(PIC_M_DATA, 0x04); // ICW3: IR2接从片.
    outb(PIC_M_DATA, 0x01); // ICW4: 8086模式, 正常EOI

    // 初始化从片
    outb(PIC_S_CTRL, 0x11); // ICW1: 边沿触发,级联8259, 需要ICW4.
    outb(PIC_S_DATA, 0x28); // ICW2: 起始中断向量号为0x28,也就是IR[8-15] 为 0x28 ~ 0x2F.
    outb(PIC_S_DATA, 0x02); // ICW3: 设置从片连接到主片的IR2引脚
    outb(PIC_S_DATA, 0x01); // ICW4: 8086模式, 正常EOI

    // 写入OCW1命令，用于操作中断屏蔽寄存器
    // 允许时钟（IRQ0）、键盘（IRQ1）、级联（IRQ2）、鼠标（IRQ12）、硬盘（IRQ14）
    outb(PIC_M_DATA, 0xF8); // 1111 1000
    outb(PIC_S_DATA, 0xAF); // 1010 1111

    sti();

    printf("pic 8259A init done\n");
}
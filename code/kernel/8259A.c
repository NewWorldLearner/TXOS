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
    // 允许级联（IRQ2），其它的外部中断统一禁止，在注册外部中断向量的时候再打开，无需再此进行侵入式修改
    outb(PIC_M_DATA, 0xFB); // 1111 1011
    outb(PIC_S_DATA, 0xFF); // 1111 1111

    sti();

    printf("pic 8259A init done\n");
}

void pic_8259A_enable(uint64_t irq)
{
    uint8_t master;
    if (irq >= 32 && irq <= 39)
    {
        master = inb(PIC_M_DATA);
        master &= ~(1 << (irq - 32));
        outb(PIC_M_DATA, master);
    }
    else if (irq >= 40 && irq <= 47)
    {
        // 允许级联
        master = inb(PIC_M_DATA);
        master &= ~(1 << 2);
        outb(PIC_M_DATA, master);
        // 允许从片上的中断
        uint8_t slave = inb(PIC_S_DATA);
        slave &= ~(1 << (irq - 40));
        outb(PIC_S_DATA, slave);
    }
}

void pic_8259A_disable(uint64_t irq)
{
    uint8_t master;
    if (irq >= 32 && irq <= 39)
    {
        master = inb(PIC_M_DATA);
        master |= 1 << (irq - 32);
        outb(PIC_M_DATA, master);
    }
    else if (irq >= 40 && irq <= 47)
    {
        uint8_t slave = inb(PIC_S_DATA);
        slave |= 1 << (irq - 40);
        outb(PIC_S_DATA, slave);
    }
}

uint64_t pic_8259A_install(uint64_t irq, void* arg)
{
    // 8259A安装中断不需要设置中断触发方式（水平还是边缘），这里什么也不做
    return 1;
}

void pic_8259A_uninstall(uint64_t irq)
{
    // 什么也不用做
}

void pic_8259A_ack(uint64_t irq)
{
    if (irq >= 32 && irq <= 39)
    {
        outb(PIC_M_CTRL, 0x20);
    }
    else if (irq >= 40 && irq <= 47)
    {
        outb(PIC_S_CTRL, 0x20);
        outb(PIC_M_CTRL, 0x20);
    }
}
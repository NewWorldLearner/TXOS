#include "../include/stdint.h"
#include "../include/interrupt.h"
#include "../include/print_kernel.h"
#include "../include/io.h"

#if APIC
#include "../include/APIC.h"
#else
#include "../include/8259A.h"
#endif

extern intr_handler idt_func_table[256];

// 等待输入缓冲区空闲
// 如果键盘输入缓冲区满的时候，说明处理器需要读取数据
// 如果此时直接向0x64端口写入数据，那么就破坏了键盘输入缓冲区的状态，可能会使键盘丢失一个输入
// 0x64作为8042的状态端口和控制端口
#define wait_keyboard_ready() while (inb(0x64) & 0x02)

// 解析三字节鼠标数据包
void parse_mouse_packet(uint8_t *packet)
{
    // 鼠标数据包的第1个字节的位0表示鼠标左键，位1表示鼠标右键，位2表示鼠标中键
    bool left_button = packet[0] & 0x01;   // 左键按下
    bool right_button = packet[0] & 0x02;  // 右键按下
    bool middle_button = packet[0] & 0x04; // 中键按下

    // 鼠标数据包的第2个字节表示x方向的位移，第3个字节表示y方向上的位移
    // PS/2鼠标协议中，位移值为​​9位补码​​，其中​位4表示x方向符号​​，​​位5表示y方向符号​​。
    // 需要注意的是，鼠标前移得到的位移是负数，因为参考的是三维坐标的y轴，而不是二维坐标上的y轴，因此将y轴坐标反转一下符合使用习惯
    bool x_sign = packet[0] & 0x10;
    bool y_sign = packet[0] & 0x20;

    int16_t x_move = (int16_t)(x_sign ? 0xFF00 | packet[1] : packet[1]); // X轴位移（有符号）
    int16_t y_move = (int16_t)(y_sign ? 0xFF00 | packet[2] : packet[2]); // Y轴位移（有符号）
    y_move = -y_move;

    // 接下来本应该是做下面的事情，但是我们这里只先简单的打印数据
    // 处理位移（例如更新光标坐标）
    // 边界检查
    // 绘制光标或触发事件

    // 这里打印的是位移，实际上应该打印的是鼠标的坐标
    printf("mouse x: %d,y: %d\n", x_move, y_move);
}

// 鼠标中断处理函数（IRQ12）
void mouse_handler()
{
    static uint8_t packet[3]; // 鼠标数据包（3字节）
    static uint8_t phase = 0; // 数据包阶段

    uint8_t data = inb(0x60); // 读取鼠标数据

    // 判断数据包起始（Byte1的Bit3=1）
    if (phase == 0 && (data & 0x08))
    {
        packet[0] = data;
        phase = 1;
    }
    else if (phase == 1)
    {
        packet[1] = data;
        phase = 2;
    }
    else if (phase == 2)
    {
        packet[2] = data;
        phase = 0;
        parse_mouse_packet(packet); // 解析完整包
    }

}

hw_int_controller mouse_int_controller =
    {
#if APIC

#else
        .enable = pic_8259A_enable,
        .disable = pic_8259A_disable,
        .install = pic_8259A_install,
        .uninstall = pic_8259A_uninstall,
        .ack = pic_8259A_ack,
#endif
};

void mouse_init()
{
    // 先使用8259A作为中断代理，简单注册中断处理函数

    // 配置控制器（优先设置正确的状态）
    wait_keyboard_ready();
    outb(0x64, 0x60); // 写入控制器配置命令
    outb(0x60, 0x47); // 配置字节：启用鼠标中断、键盘接口、扫描码转换

    // 启用PS/2鼠标端口
    wait_keyboard_ready();
    outb(0x64, 0xA8); // 命令0xA8：启用第二个PS/2端口

    // 发送启用数据流命令并等待ACK
    wait_keyboard_ready();
    outb(0x64, 0xD4); // 下一命令发送到鼠标
    wait_keyboard_ready();
    outb(0x60, 0xF4); // 0xF4: 启用数据流模式

    register_irq(44, NULL, mouse_handler, 0, &mouse_int_controller, "ps/2 mouse");

    printf("mouse init done\n");
}
#include "../include/timer.h"
#include "../include/io.h"
#include "../include/print_kernel.h"
#include "../include/interrupt.h"

#if APIC
#include "../include/APIC.h"
#else
#include "../include/8259A.h"
#endif

// 注意到计数器初值占16位，最大值为65535，频率设置过高（比如100）会导致bochs虚拟机的打印出现问题，还是代码存在bug呢？
#define IRQ0_FREQUENCY 35
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY)

volatile uint64_t jiffies = 0;

void timer_handler()
{
    jiffies++;
}

hw_int_controller timer_int_controller =
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

// 使用8253来作为计数器
void timer_init()
{
    // 8253的计数器0的工作方式，先向0x43端口写入控制命令，然后通过0x40端口先写入计数器初值的低8位，然后再写入高8位
    // 0x43端口的位7-6表示选择计数器，位5-4表示选择读写方式（我们选择11，先读写低字节，再读写高字节），位3-2表示工作方式（我们选择工作方式2），位0表示数制格式（0表示二进制）
    outb(0x43, (0 << 6) | (3 << 4) | (2 << 1) | 0);
    outb(0x40, (uint8_t)COUNTER0_VALUE);
    outb(0x40, (uint8_t)(COUNTER0_VALUE >> 8));

    register_irq(32, NULL, timer_handler, 0, &timer_int_controller, "timer");

    printf("timer init done.\n");
}
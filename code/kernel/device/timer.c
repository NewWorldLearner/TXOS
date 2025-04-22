#include "../include/timer.h"
#include "../include/io.h"
#include "../include/print_kernel.h"
#include "../include/interrupt.h"

#define IRQ0_FREQUENCY 10
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY)

extern intr_handler idt_func_table[256];

void timer_handler()
{
    static int i = 0;
    printf("time interrupt: %d\n", i++);
    outb(0x20, 0x20);
}

// 使用8253来作为计数器
void timer_init()
{
    // 8253的计数器0的工作方式，先向0x43端口写入控制命令，然后通过0x40端口先写入计数器初值的低8位，然后再写入高8位
    // 0x43端口的位7-6表示选择计数器，位5-4表示选择读写方式（我们选择11，先读写低字节，再读写高字节），位3-2表示工作方式（我们选择工作方式2），位0表示数制格式（0表示二进制）
    outb(0x43, (0 << 6) | (3 << 4) | (2 << 1) | 0);
    outb(0x40, (uint8_t)COUNTER0_VALUE);
    outb(0x40, (uint8_t)(COUNTER0_VALUE >> 8));

    idt_func_table[32] = timer_handler;
    int i = 0;
    printf("tomer init done.\n");
}
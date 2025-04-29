#ifndef _8259A_H_
#define _8259A_H_

// 8259A的端口
#define PIC_M_CTRL 0x20 // 主片的控制端口是0x20
#define PIC_M_DATA 0x21 // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0 // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1 // 从片的数据端口是0xa1

void pic_8259A_init();

void pic_8259A_enable(uint64_t irq);
void pic_8259A_disable(uint64_t irq);
uint64_t pic_8259A_install(uint64_t irq, void *arg);
void pic_8259A_uninstall(uint64_t irq);
void pic_8259A_ack(uint64_t irq);

#endif
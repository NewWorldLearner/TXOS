#ifndef _IO_H
#define _IO_H
#include "stdint.h"

// 从端口port读取一个字节
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

// 向端口port写入一个字节
static inline void outb(uint16_t port, uint8_t data)
{
    // 变量data绑定到寄存器eax，变量port绑定到寄存器edx
    // N是立即数约束，表示0到255之间的整数，如果传参时port的值小于等于255，那么就可用不用通过寄存器dx传参，直接将端口号硬编码到指令中
    // b表示低1字节，w表示低2字节，h表示高2字节，l表示4字节
    // 端口号范围是0-65535
    asm volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

// 从端口port读取4字节
static inline uint32_t inl(uint16_t port)
{
    uint32_t data = 0;
    // mfence指令告诉cpu按顺序执行指令，不要乱序执行
    asm __volatile__("inl %%dx, %0 \n\t"
                     "mfence \n\t"
                     : "=a"(data)
                     : "d"(port)
                     : "memory");
    return data;
}

// 向端口port写入4字节的data
static inline void outl(uint16_t port, uint32_t data)
{
    asm __volatile__("outl %0, %%dx \n\t"
                     "mfence \n\t"
                     :
                     : "a"(data), "d"(port)
                     : "memory");
}

// 将从端口port读入的word_cnt个字写入addr
static inline void insw(uint16_t port, void *addr, uint32_t word_cnt)
{
    // D表示寄存器rdi
    // +表示寄存器或内存可读可写
    // memory用于通知gcc内存可能会被修改
    asm volatile("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d"(port) : "memory");
}

// 将addr处起始的word_cnt个字写入端口port
static inline void outsw(uint16_t port, const void *addr, uint32_t word_cnt)
{
    // S表示寄存器rsi
    asm volatile("cld; rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port): "memory");
}

#define nop() asm volatile("nop	\n\t")

#endif
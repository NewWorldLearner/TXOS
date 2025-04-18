#ifndef _CPU_H_
#define _CPU_H_

// cpuid指令可以获取cpu相关信息，cpuid指令的输入为EAX和ECX(主要是EAX，ECX应该比较少用到)，输出为EAX、EBX、ECX和EDX
inline static void get_cpuid(unsigned int Mop, unsigned int Sop, unsigned int *a, unsigned int *b, unsigned int *c, unsigned int *d)
{
    __asm__ __volatile__("cpuid	\n\t"
                         : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                         : "0"(Mop), "2"(Sop));
}

#endif
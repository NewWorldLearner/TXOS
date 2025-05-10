#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "../../include/stdint.h"

// 支持0个参数的系统调用
// 需要注意的是，由于我们定义的是一个宏，在宏中使用的内联汇编，内联汇编中定义得有标签，这就意味着当宏被多次调用时，相应的标签就会被多次定义
// 显然会导致歧义，因此在标签后面加上%=，这就使得该标签会成为局部标签，消除了歧义
// 或者使用数字作为标签，比如1：  然后使用1f，这里的f表示表示前向局部引用
#define _syscall0(NUMBER) ({                                  \
    uint64_t retval;                                          \
    asm volatile(                                             \
        "pushq %%r10       \n\t"                              \
        "pushq %%r11        \n\t"                             \
        "leaq sysexit_return_address%=(%%rip), %%r10    \n\t" \
        "movq %%rsp, %%r11   \n\t"                            \
        "sysenter           \n\t"                             \
        "sysexit_return_address%=: \n\t"                      \
        "xchgq %%rdx,   %%r10    \n\t"                        \
        "xchgq %%rcx,   %%r11    \n\t"                        \
        "popq %%r11             \n\t"                         \
        "popq %%r10              \n\t"                        \
        : "=a"(retval)                                        \
        : "a"(NUMBER)                                        \
        : "memory");                                          \
    retval;                                                   \
})

// 支持1个参数的系统调用，参数保存在寄存器rdi中
#define _syscall1(NUMBER, arg1) ({                            \
    uint64_t retval;                                          \
    asm volatile(                                             \
        "pushq %%r10       \n\t"                              \
        "pushq %%r11        \n\t"                             \
        "leaq sysexit_return_address%=(%%rip), %%r10    \n\t" \
        "movq %%rsp, %%r11   \n\t"                            \
        "sysenter           \n\t"                             \
        "sysexit_return_address%=: \n\t"                      \
        "xchgq %%rdx,   %%r10    \n\t"                        \
        "xchgq %%rcx,   %%r11    \n\t"                        \
        "popq %%r11             \n\t"                         \
        "popq %%r10              \n\t"                        \
        : "=a"(retval)                                        \
        : "a"(NUMBER), "D"(arg1)                              \
        : "memory");                                          \
    retval;                                                   \
})

// 支持2个参数的系统调用，参数保存在寄存器rdi和rsi中
#define _syscall2(NUMBER, arg1, arg2) ({                      \
    uint64_t retval;                                          \
    asm volatile(                                             \
        "pushq %%r10       \n\t"                              \
        "pushq %%r11        \n\t"                             \
        "leaq sysexit_return_address%=(%%rip), %%r10    \n\t" \
        "movq %%rsp, %%r11   \n\t"                            \
        "sysenter           \n\t"                             \
        "sysexit_return_address%=: \n\t"                      \
        "xchgq %%rdx,   %%r10    \n\t"                        \
        "xchgq %%rcx,   %%r11    \n\t"                        \
        "popq %%r11             \n\t"                         \
        "popq %%r10              \n\t"                        \
        : "=a"(retval)                                        \
        : "a"(NUMBER), "D"(arg1), "S"(arg2)                   \
        : "memory");                                          \
    retval;                                                   \
})

// 支持3个参数的系统调用，参数保存在寄存器rdi、rsi和rdx中
#define _syscall3(NUMBER, arg1, arg2, arg3) ({                \
    uint64_t retval;                                          \
    asm volatile(                                             \
        "pushq %%r10       \n\t"                              \
        "pushq %%r11        \n\t"                             \
        "leaq sysexit_return_address%=(%%rip), %%r10    \n\t" \
        "movq %%rsp, %%r11   \n\t"                            \
        "sysenter           \n\t"                             \
        "sysexit_return_address%=: \n\t"                      \
        "xchgq %%rdx,   %%r10    \n\t"                        \
        "xchgq %%rcx,   %%r11    \n\t"                        \
        "popq %%r11             \n\t"                         \
        "popq %%r10              \n\t"                        \
        : "=a"(retval)                                        \
        : "a"(NUMBER), "D"(arg1), "S"(arg2), "d"(arg3)        \
        : "memory");                                          \
    retval;                                                   \
})

// 支持4个参数的系统调用，参数保存在寄存器rdi、rsi、rdx和rcx中
#define _syscall4(NUMBER, arg1, arg2, arg3, arg4) ({              \
    uint64_t retval;                                              \
    asm volatile(                                                 \
        "pushq %%r10       \n\t"                                  \
        "pushq %%r11        \n\t"                                 \
        "leaq sysexit_return_address%=(%%rip), %%r10    \n\t"     \
        "movq %%rsp, %%r11   \n\t"                                \
        "sysenter           \n\t"                                 \
        "sysexit_return_address%=: \n\t"                          \
        "xchgq %%rdx,   %%r10    \n\t"                            \
        "xchgq %%rcx,   %%r11    \n\t"                            \
        "popq %%r11             \n\t"                             \
        "popq %%r10              \n\t"                            \
        : "=a"(retval)                                            \
        : "a"(NUMBER), "D"(arg1), "S"(arg2), "d"(arg3), "c"(arg4) \
        : "memory");                                              \
    retval;                                                       \
})

enum SYSCALL_NR
{
    SYS_WRITE,
    SYS_YILED
};

uint64_t write(char *str);

#endif
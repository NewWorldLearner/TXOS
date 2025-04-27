#ifndef _THREAD_H_
#define _THREAD_H_

#include "memory.h"
#include "interrupt.h"


#define KERNEL_CS (0x08)
#define KERNEL_DS (0x10)

#define STACK_SIZE 32768

// 进程的状态
enum task_status
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// 进程的标志，是用户还是内核，是线程还是进程
enum task_flag
{
    KERNEL_TASK,
    USER_TASK,
    KERNEL_THREAD,
    USER_THREAD
};


// 进程的内存管理，用于记录进程的地址空间和程序的空间分布
struct mm_struct
{
    pml4t_t *pgd; // page table point

    uint64_t start_code, end_code;
    uint64_t start_data, end_data;
    uint64_t start_rodata, end_rodata;
    uint64_t start_brk, end_brk;
    uint64_t start_stack;
};


// 线程结构体用于切换线程的时候，保存线程的上下文，其实线程的上下文被隐式地保存在内核栈中，只需要保存rsp就可以了。
// 对于rip，因为在调用switch_to函数的时候，本身就在栈中压入了返回地址，因此rip保存或者不保存都是可以的。
// 最重要的就是保存rsp
struct thread_struct
{
    uint64_t rip;
    uint64_t rsp;           // 内核栈指针

    uint64_t rsp0;          // 内核栈的栈底
    uint64_t fs;
    uint64_t gs;

    uint64_t cr2;
    uint64_t trap_nr;
    uint64_t error_code;
};

struct tss_struct
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomapbaseaddr;
} __attribute__((packed));


// PCB占用32KB的内存，在PCB的底端（低地址）存放进程信息，在PCB的顶端用作进程的内核栈
// 最顶端存放的是中断栈，中断栈用于在中断发生的时候，进行上下文保护，中断栈下面就是内核可以正常使用的栈
// 在进行进程或者线程切换的时候，也需要保存上下文，因此会将所有寄存器进行压栈保护，为了方便就将这些寄存器封装起来，当作线程栈
struct task_struct
{
    struct List list;                           // 串联PCB的链表
    volatile enum task_status status;           // 进程状态
    uint64_t flag;                             // 用于标识是线程还是进程，是内核还是用户

    struct mm_struct *mm;                       // 程序的空间分布
    struct thread_struct *thread;               // 线程信息
    struct tss_struct *tss;                     // tss的地址
    struct pt_regs *regs;                       // regs的地址

    uint64_t vaddr;                        // 程序的线性地址

    int64_t pid;                                // 进程ID

    int64_t priority;                           // 进程优先级

    int64_t tickets;                            // 进程的票数
};

union task_union
{
    struct task_struct task;
    uint64_t stack[STACK_SIZE / sizeof(uint64_t)];
} __attribute__((aligned(8))); // 8Bytes align


void kernel_process_init();

int thread_create(uint64_t (*function)(uint64_t), uint64_t arg);

void set_tss64(struct tss_struct *tss);

#endif

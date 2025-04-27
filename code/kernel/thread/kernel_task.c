#include "../include/thread.h"
#include "../include/memory.h"
#include "../include/list.h"
#include "../include/interrupt.h"
#include "../include/print_kernel.h"
#include "../include/list.h"

union task_union kernel_task_union __attribute__((__section__(".data.init_task")));

struct mm_struct kernel_mm;
struct thread_struct kernel_thread;
struct tss_struct kernel_tss;

extern uint64_t _stack_start;

extern char _text;
extern char _etext;
extern char _data;
extern char _edata;
extern char _rodata;
extern char _erodata;
extern char _bss;
extern char _ebss;
extern char _end;

// 初始化PCB中的task
inline static void init_kernel_task()
{
    struct task_struct *task = (struct task_struct *)&kernel_task_union;
    list_init(&task->list);
    task->status = TASK_RUNNING;
    task->flag = KERNEL_TASK;
    task->mm = &kernel_mm;
    task->thread = &kernel_thread;
    task->vaddr = 0xffff800000000000;
    task->pid = 0;
    task->priority = 0;
    task->tickets = 10;
}

inline static void init_kernel_thread()
{
    kernel_thread.fs = KERNEL_DS;
    kernel_thread.gs = KERNEL_DS;
    kernel_thread.rsp0 = (uint64_t)(kernel_task_union.stack + STACK_SIZE / sizeof(uint64_t));
    // 内核进程的rsp在进程运行的时候就已经改变了，因此这里初始化其实没有什么意义
    kernel_thread.rsp = (uint64_t)(kernel_task_union.stack + STACK_SIZE / sizeof(uint64_t));
    kernel_thread.cr2 = 0;
    kernel_thread.error_code = 0;
    kernel_thread.trap_nr = 0;
}

inline static void init_kernel_mm()
{
    kernel_mm.pgd = (pml4t_t *)Global_CR3;
    kernel_mm.start_code = memory_management_struct.start_code;
    kernel_mm.end_code = memory_management_struct.end_code;
    kernel_mm.start_data = (uint64_t)&_data;
    kernel_mm.end_data = memory_management_struct.end_data;
    kernel_mm.start_rodata = (uint64_t)&_rodata;
    kernel_mm.end_rodata = (uint64_t)&_erodata;
    kernel_mm.start_brk = 0;
    kernel_mm.end_brk = memory_management_struct.end_brk;
    kernel_mm.start_stack = _stack_start;
}

inline static void init_kernel_tss()
{
    kernel_tss.reserved0 = 0;
    kernel_tss.rsp0 = (uint64_t)(kernel_task_union.stack + STACK_SIZE / sizeof(uint64_t));
    kernel_tss.rsp1 = (uint64_t)(kernel_task_union.stack + STACK_SIZE / sizeof(uint64_t));
    kernel_tss.rsp2 = (uint64_t)(kernel_task_union.stack + STACK_SIZE / sizeof(uint64_t));
    kernel_tss.reserved1 = 0;
    kernel_tss.ist1 = 0xffff800000007c00;
    kernel_tss.ist2 = 0xffff800000007c00;
    kernel_tss.ist3 = 0xffff800000007c00;
    kernel_tss.ist4 = 0xffff800000007c00;
    kernel_tss.ist5 = 0xffff800000007c00;
    kernel_tss.ist6 = 0xffff800000007c00;
    kernel_tss.ist7 = 0xffff800000007c00;
    kernel_tss.reserved2 = 0;
    kernel_tss.reserved3 = 0;
    kernel_tss.iomapbaseaddr = 0;
}


// 下面要实现的代码时初始化内核进程
// 对于内核进程来说，需要初始化PCB，需要初始化TSS
// 我们将内核PCB安排在内核程序的后面
void kernel_process_init()
{
    init_kernel_task();
    init_kernel_mm();
    init_kernel_tss();
    // 值得注意的是，每个进程都有自己的TSS数据，但是需要放到TSS中才能在中断的时候使用到
    set_tss64(&kernel_tss);

    printf("init kernel task done!\n");
}



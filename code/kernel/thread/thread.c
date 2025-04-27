#include "../include/interrupt.h"
#include "../include/string.h"
#include "../include/thread.h"
#include "../include/print_kernel.h"

extern union task_union kernel_task_union;

void init_pcb_task(struct task_struct *task, uint64_t priority, uint64_t tickets)
{
    list_init(&task->list);
    task->thread = (struct thread_struct *)(task + 1);
    task->mm = (struct mm_struct *)(task->thread + 1);
    task->tss = (struct tss_struct *)(task->mm + 1);
    task->regs = (struct pt_regs *)((uint64_t)task + STACK_SIZE - sizeof(struct pt_regs));
    task->status = TASK_READY;
    task->flag = KERNEL_TASK;
    task->vaddr = 0xffff800000000000; // 内核地址
    task->pid = 0;
    task->priority = priority;
    task->tickets = tickets;

    // 将task加入就绪列表
    list_insert_before(&kernel_task_union.task.list, &task->list);
}

void init_pcb_thread(struct task_struct *task)
{
    struct thread_struct *thread = task->thread;
    memset(thread, 0, sizeof(struct thread_struct));
    thread->rsp0 = (uint64_t)task + STACK_SIZE;
    thread->rsp = (uint64_t)task + STACK_SIZE - sizeof(struct pt_regs);
    thread->rip = task->regs->rip;
}

void init_pcb_mm(struct mm_struct *mm)
{
    memset(mm, 0, sizeof(struct mm_struct));
}

void init_pcb_tss(struct task_struct *task)
{
    struct tss_struct *tss = task->tss;
    tss->rsp0 = (uint64_t)task + STACK_SIZE;
    tss->rsp1 = (uint64_t)task + STACK_SIZE;
    tss->rsp2 = (uint64_t)task + STACK_SIZE;

    tss->ist1 = (uint64_t)task + STACK_SIZE;
    tss->ist2 = (uint64_t)task + STACK_SIZE;
    tss->ist3 = (uint64_t)task + STACK_SIZE;
    tss->ist4 = (uint64_t)task + STACK_SIZE;
    tss->ist5 = (uint64_t)task + STACK_SIZE;
    tss->ist6 = (uint64_t)task + STACK_SIZE;
    tss->ist7 = (uint64_t)task + STACK_SIZE;

    tss->iomapbaseaddr = 0;

    tss->reserved0 = 0;
    tss->reserved1 = 0;
    tss->reserved2 = 0;
    tss->reserved3 = 0;
}

void init_pcb_regs(struct pt_regs *regs, uint64_t (*fn)(uint64_t), uint64_t arg)
{
    regs->rbx = (uint64_t)fn;
    regs->rdx = (uint64_t)arg;

    regs->ds = KERNEL_DS;
    regs->es = KERNEL_DS;
    regs->cs = KERNEL_CS;
    regs->ss = KERNEL_DS;
}

//---------------------------以上函数用于辅助实现PCB的初始化---------------------------

//----------------------------------下面的函数用于辅助实现创建一个进程---------------------

// 理论上进程结束的时候，我们应该销毁线程，把线程从PCB链表中移出来
uint64_t do_exit(uint64_t code)
{
    printf("exit task is running,arg:%x\n", code);
    while (1)
        ;
}

extern void kernel_thread_func(void);
__asm__(
    "kernel_thread_func:	\n\t"
    "	popq	%r15	\n\t"
    "	popq	%r14	\n\t"
    "	popq	%r13	\n\t"
    "	popq	%r12	\n\t"
    "	popq	%r11	\n\t"
    "	popq	%r10	\n\t"
    "	popq	%r9	\n\t"
    "	popq	%r8	\n\t"
    "	popq	%rbx	\n\t"
    "	popq	%rcx	\n\t"
    "	popq	%rdx	\n\t"
    "	popq	%rsi	\n\t"
    "	popq	%rdi	\n\t"
    "	popq	%rbp	\n\t"

    "	popq	%rax	\n\t"
    "	movq	%rax,	%ds	\n\t"

    "	popq	%rax		\n\t"
    "	movq	%rax,	%es	\n\t"

    "	popq	%rax		\n\t"

    "	addq	$0x38,	%rsp	\n\t"
    "	movq	%rdx,	%rdi	\n\t"
    "	callq	*%rbx		\n\t"
    "	movq	%rax,	%rdi	\n\t"
    "	callq	do_exit		\n\t");

// 创建线程其实就是创建一块PCB并初始化
// 在执行线程函数之前，首先执行一段引导程序，在执行完线程函数之后，再执行一段退出程序
int thread_create(uint64_t (*function)(uint64_t), uint64_t arg)
{

    struct Page *p = alloc_pages(ZONE_NORMAL, 1, PG_PTable_Maped | PG_Kernel);

    struct task_struct *task = (struct task_struct *)Phy_To_Virt(p->PHY_address);

    // 初始化task_struct
    init_pcb_task(task, 10, 10);

    // 初始化regs
    init_pcb_regs(task->regs, function, arg);

    task->regs->rip = (uint64_t)kernel_thread_func;

    // 初始化thread_struct
    init_pcb_thread(task);

    // 初始化mm_struct
    init_pcb_mm(task->mm);

    // 初始化tss_struct
    init_pcb_tss(task);

}



#include "../include/interrupt.h"
#include "../include/string.h"
#include "../include/thread.h"
#include "../include/print_kernel.h"
#include "../include/process.h"

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

// 曾经在调试的时候，先是怀疑错误出现在kernel_thread_func中，然后在下面的代码中的开始部分插入了jmp kernel_thread_func
// 结果是死循环，符合预期
// 然后又将jmp指令插入到第1条pop指令之后，结果就导致了pop指令不断被执行，直到产生缺页异常
extern void kernel_thread_func(void);
__asm__(
    "kernel_thread_func:	\n\t"

    "	popq	%rax		\n\t"
    "	movq	%rax,	%es	\n\t"

    "	popq	%rax	\n\t"
    "	movq	%rax,	%ds	\n\t"

    "	popq	%r15	\n\t"
    "	popq	%r14	\n\t"
    "	popq	%r13	\n\t"
    "	popq	%r12	\n\t"
    "	popq	%r11	\n\t"
    "	popq	%r10	\n\t"
    "	popq	%r9	    \n\t"
    "	popq	%r8	    \n\t"

    "	popq	%rbp	\n\t"
    "	popq	%rdi	\n\t"
    "	popq	%rsi	\n\t"

    "	popq	%rdx	\n\t"
    "	popq	%rcx	\n\t"
    "	popq	%rbx	\n\t"
    "	popq	%rax	\n\t"

    "	addq	$0x30,	%rsp	\n\t"
    "	movq	%rdx,	%rdi	\n\t"
    "	callq	*%rbx		\n\t"
    "	movq	%rax,	%rdi	\n\t"
    "	callq	do_exit		\n\t");

// 创建线程其实就是创建一块PCB并初始化
// 在执行线程函数之前，首先执行一段引导程序，在执行完线程函数之后，再执行一段退出程序
// 理论上来说，mm、thread、tss结构体是应该动态申请内存的，但是kmalloc相关的函数似乎还存在bug，所以后续再修改
struct task_struct *thread_create(uint64_t (*function)(uint64_t*), uint64_t *arg)
{
    struct task_struct *task = kmalloc(SIZE_32K, PG_Kernel);

    // 复制task_struct结构体
    *task = *current;
    // 接下来初始化一些特异化信息，与被复制的线程的信息不同
    list_insert_before(&(current->list), &(task->list));
    task->tid++;
    task->priority = 10;
    task->tickets = 10;

    // 复制thread_struct结构体，再进行初始化
    struct thread_struct *thread = (struct thread_struct *)(task + 1);
    task->thread = thread;
    memcpy(thread, current->thread, sizeof(struct thread_struct));
    thread->rsp0 = (uint64_t)task + STACK_SIZE;
    thread->rsp = thread->rsp0 - sizeof(struct pt_regs);
    thread->rip = (uint64_t)kernel_thread_func;

    // 复制mm_struct结构体，mm中保存的页表肯定是要复制的，其它的暂时不重要
    task->mm = (struct mm_struct *)(task->thread + 1);
    *(task->mm) = *(current->mm);

    // 初始化tss，tss应该是互不相同的，tss中保存的值与pcb有关，因此tss无需复制
    task->tss = (struct tss_struct *)(task->mm + 1);
    init_pcb_tss(task);

    // 初始化regs，因为线程可能是内核线程或者用户线程，但目前来说，它还属于内核线程
    task->regs = (struct pt_regs*)(thread->rsp0 - sizeof(struct pt_regs));

    task->regs->rbx = (uint64_t)function;
    task->regs->rdx = (uint64_t)arg;
    task->regs->rsp = thread->rsp;
    task->regs->rip = thread->rip;
    task->regs->rflags = (1 << 9); // 允许中断

    task->regs->cs = KERNEL_CS;
    task->regs->ds = KERNEL_DS;
    task->regs->es = KERNEL_DS;
    task->regs->ss = KERNEL_DS;

    return task;
}



//-----------------------以下函数用于辅助实现进程的切换-----------------------

void set_tss64(struct tss_struct *tss)
{
    *(uint64_t *)(TSS64_Table + 1) = tss->rsp0;
    *(uint64_t *)(TSS64_Table + 3) = tss->rsp1;
    *(uint64_t *)(TSS64_Table + 5) = tss->rsp2;

    *(uint64_t *)(TSS64_Table + 9) = tss->ist1;
    *(uint64_t *)(TSS64_Table + 11) = tss->ist2;
    *(uint64_t *)(TSS64_Table + 13) = tss->ist3;
    *(uint64_t *)(TSS64_Table + 15) = tss->ist4;
    *(uint64_t *)(TSS64_Table + 17) = tss->ist5;
    *(uint64_t *)(TSS64_Table + 19) = tss->ist6;
    *(uint64_t *)(TSS64_Table + 21) = tss->ist7;
}

#define PUSHALL                                      \
    "pushfq                                    \n\t" \
    "pushq %%r15                               \n\t" \
    "pushq %%r14                               \n\t" \
    "pushq %%r13                               \n\t" \
    "pushq %%r12                               \n\t" \
    "pushq %%r11                               \n\t" \
    "pushq %%r10                               \n\t" \
    "pushq %%r9                                \n\t" \
    "pushq %%r8                                \n\t" \
    "pushq %%rbp                               \n\t" \
    "pushq %%rdi                               \n\t" \
    "pushq %%rsi                               \n\t" \
    "pushq %%rdx                               \n\t" \
    "pushq %%rcx                               \n\t" \
    "pushq %%rbx                               \n\t" \
    "pushq %%rax                               \n\t"

#define POPALL                                       \
    "popq %%rax                                \n\t" \
    "popq %%rbx                                \n\t" \
    "popq %%rcx                                \n\t" \
    "popq %%rdx                                \n\t" \
    "popq %%rsi                                \n\t" \
    "popq %%rdi                                \n\t" \
    "popq %%rbp                                \n\t" \
    "popq %%r8                                 \n\t" \
    "popq %%r9                                 \n\t" \
    "popq %%r10                                \n\t" \
    "popq %%r11                                \n\t" \
    "popq %%r12                                \n\t" \
    "popq %%r13                                \n\t" \
    "popq %%r14                                \n\t" \
    "popq %%r15                                \n\t" \
    "popfq                                     \n\t"

// 下面的函数中，首先对cur进程的上下文进行压栈保护
// 然后cur进程的rsp保存在当前进程的PCB中
// 加载next进程的rsp
// 设定cur进程的rip并存放在cur进程的PCB中
// 将next进程的rip压入next的栈中
// 使用ret指令执行next进程
// 进入swict_to时在cur的栈中压入了返回地址，等cur下次在执行的时候，就会使cur::swict_to函数执行完,因此rip被自动保存
// 这里我们手动保存rip的话，好像也并没有关系，因为第1次ret执行的是被保存的rip，第2次ret会回到switch的调用者，手动保存rip，那么就无需强制rip的位置了
// 因此可以得出结论，最重要的是保存esp
void switch_to(struct task_struct *cur, struct task_struct *next)
{
    // 首先切换TSS
    set_tss64(next->tss);
    // 在这里激活页表并不会导致错误，因为switch_to函数运行于内核空间，而内核空间是共享给所有用户的，因此激活用户的页表并不会影响内核代码的执行
    page_dir_activate(next);

    // 然后再切换上下文
    __asm__ __volatile__(
        PUSHALL
        "movq	%%rsp,	%0	\n\t"
        "movq	%2,	%%rsp	\n\t"
        "leaq	1f(%%rip),	%%rax	\n\t"
        "movq	%%rax,	%1	\n\t"
        "pushq	%3		\n\t"
        "ret	\n\t"
        "1:	\n\t" 
        POPALL
        : "=m"(cur->thread->rsp), "=m"(cur->thread->rip)
        : "m"(next->thread->rsp), "m"(next->thread->rip), "D"(cur), "S"(next)
        : "memory");
}


// 实现的进程调度，现在我们并不设计进程调度算法，只是简单的将当前进程换下处理器
// 在调用schedule之前，我们首先要找到当前进程的PCB，当用户程序调用schedule时，得到的cur是不正确的
void schedule()
{
    struct task_struct *cur = get_current();
    struct task_struct *next = container_of(list_next(&current->list), struct task_struct, list);

    cli();
    switch_to(cur, next);
    sti();
}
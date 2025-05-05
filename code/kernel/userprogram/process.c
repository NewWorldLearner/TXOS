#include "../include/process.h"
#include "../include/memory.h"
#include "../include/thread.h"
#include "../include/string.h"
#include "../include/debug.h"
#include "../include/print_kernel.h"


#define USER_VADDR_START 0x8048000
// 128TB
#define USER_VADDR_END 0x00007FFFFFFFFFFF
// 栈上面用于保存环境变量，参数等信息
#define USER_VADDR_STACK (USER_VADDR_START + 0x100000)

void create_user_vaddr_bitmap(struct task_struct *task)
{
    // 用户空间128TB，物理页大小为2MB，需要使用的位图为8MB，这也有点过于夸张了
    // 1个字节可以表示16M内存，1024字节可以表示16GB内存
    // 先设计用户进程最多占16GB吧
    task->vaddr.vaddr_bitmap.bits = (uint64_t*)kmalloc(1024, PG_Kernel);
    task->vaddr.vaddr_bitmap.length = 1024;
    task->vaddr.vaddr_start = USER_VADDR_START;
    memset(task->vaddr.vaddr_bitmap.bits, 0x00, 1024);
}

uint64_t *create_page_dir()
{

    // uint64_t *page_dir_vaddr = kmalloc(SIZE_4K, PG_Kernel);
    uint64_t *page_dir_vaddr = get_vaddr(PF_KERNEL, 1, PG_Kernel);
    uint64_t *kernel_dir = Phy_To_Virt(Global_CR3);

    ASSERT(page_dir_vaddr);

    // 曾经在这里犯下过一个错误，不小心把内核页表的高半部分复制到用户页表的低半部分，导致后面激活用户页表的时候程序直接崩溃
    memcpy(page_dir_vaddr + 256, kernel_dir + 256, 256 * 8);

    // 让页全局目录的最后一个页目录项保存页全局目录的物理地址
    page_dir_vaddr[511] = Virt_To_Phy(page_dir_vaddr) | PAGE_USER_Dir;

    return page_dir_vaddr;
}

void page_dir_activate(struct task_struct *task)
{
    uint64_t pagedir_phy_addr = Virt_To_Phy(task->mm->page_dir);
    // 更新页目录寄存器cr3,使新页表生效
    asm volatile("movq %0, %%cr3" : : "r"(pagedir_phy_addr) : "memory");
}

void build_process_context()
{
    struct pt_regs *regs = current->regs;
    regs->rdx = USER_VADDR_START;
    regs->rcx = USER_VADDR_STACK;
    regs->ds = 0x30;
    regs->es = 0x30;
}

void load_user_process(void *function)
{
    struct Page *page = alloc_pages(ZONE_NORMAL, 1, PG_Kernel);

    pagetable_add(USER_VADDR_START, page->PHY_address);

    memcpy((void*)USER_VADDR_START, function, SIZE_4K);
}

// 在线程首次执行的时候，我们为线程初始化了一次上下文，线程现在执行了start_process函数
// 在该函数中，线程要变成进程，于是我们不得不为进程再次构建一次上下文，然后让进程进入用户态
// 可以省略掉为线程初始化上下文吗？只构建一次上下文，让线程一开始直接进入用户态，直接变成进程？
// 第1次构建上下文是用于线程执行，第2次构上下文是用于进程执行，能否只构建一次上下文呢？
// 当线程执行的时候，栈指针先是回到栈底rsp0，于是执行线程函数start_process，栈指针此时指向于中断栈的空间，然后我们又直接通过regs操控了中断栈，导致数据变得混乱起来
// 因此如果我们想要构建上下文，首要任务就是让栈指针跳过regs结构体
// 由于我们是通过线程来变成进程的，因此我们一共需要构建两次上下文，由于进程的上下文保存在PCB顶端，因此而线程函数杠开始执行的时候，栈指针可是指向的PCB顶端
// 因此进程上下文和线程函数就共同使用了一块区域，这就导致了冲突，使得数据混乱，因此，在线程函数中，要么我们直接操纵栈指针，使其移动到进程上下文下面，然后再构建上下文
// 要么我们就临时使用上下文区域作为线程函数的栈，等做完必要的操作后，然后移动栈指针到进程上下文下面，最后跳转到另一个函数中来构建上下文
// 归根结底就是我们不能把用于构建上下文的区域作为栈区，务必要跳过上下文区域之后才能使用栈
// 我们选择在另一个函数中构建进程上下文，这样看起来安全性更高一些
uint64_t start_process(uint64_t *user_process)
{

    struct task_struct *task = get_current();
    task->thread->rsp = task->thread->rsp0 - sizeof(struct pt_regs);

    task->thread->rip = (uint64_t)system_exit;
    load_user_process(user_process);

    __asm__ __volatile__("movq	%0,	%%rsp	\n\t"
                         "pushq	%1		\n\t"
                         "jmp build_process_context \n\t"
                         "retq           \n\t"
                         ::
                         "m"(current->thread->rsp),
                         "m"(current->thread->rip) : "memory");
    return 1;
}

void process_creat(uint64_t *user_process)
{

    struct task_struct *task = thread_create(start_process, user_process);

    create_user_vaddr_bitmap(task);

    task->mm->page_dir = create_page_dir();

    // 是否需要关中断呢？

    // 将用户进程加入到就绪列表中

    // 将用户进程加入到所有进程列表中

    // 恢复中断状态
}
#include "include/syscall-init.h"
#include "include/print_kernel.h"
#include "include/stdint.h"
#include "lib/user/syscall.h"


// 暂时只支持128个系统调用
syscall_handler syscall_func_table[128];

void init_syscall()
{
    syscall_func_table[SYS_WRITE] = sys_write;
}
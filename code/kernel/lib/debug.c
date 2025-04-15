#include "../include/debug.h"
#include "../include/print_kernel.h"

/* 打印文件名,行号,函数名,条件并使程序悬停 */
void panic_spin(char* filename, int line, const char* func, const char* condition)
{
   // 这里实际上还应该关闭中断，以免在打印错误信息时被中断打乱，后续需要加上
   printf("filename:%s line:%d function:%s condition:%s\n",filename, line, func, condition);
   while(1);
}

#include "syscall.h"

uint64_t write(char *str)
{
    return _syscall(SYS_WRITE, str);
}
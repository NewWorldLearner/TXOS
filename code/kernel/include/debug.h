#ifndef _DEBUG_H
#define _DEBUG_H
void panic_spin(char *filename, int line, const char *func, const char *condition);

// 宏也能够支持可变参数，对于宏中的可变参数，在宏的替换列表中，使用__VA_ARGS__来代替...
// __VA_ARGS__的本质是个字符串指针
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)


#ifdef NDEBUG
#define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION)                               \
    if (CONDITION)                                      \
    {                                                   \
    }                                                   \
    else                                                \
    {                                                   \
        /* 符号#让编译器将宏的参数转化为字符串字面量 */ \
        PANIC(#CONDITION);                              \
    }
#endif /*__NDEBUG */

#endif /* _NDEBUG_H */
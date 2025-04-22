
#include <stdarg.h>
#include "include/stdint.h"
#include "include/string.h"
#include "include/print_kernel.h"
#include "include/font.h"

struct position Pos;

void init_screen()
{

    Pos.XResolution = 1440;
    Pos.YResolution = 900;

    Pos.XCharPosition = 0;
    Pos.YCharPosition = 0;

    Pos.XCharSize = 8;
    Pos.YCharSize = 16;

    Pos.FB_addr = (int *)0xffff800003000000;
    Pos.FB_length = Pos.XResolution * Pos.YResolution * 4;
}

// fb是帧缓存区首地址，Xsize表示屏幕的长度，x表示字符的显示位置的像素偏移量，y同理
void putchar(unsigned int *fb, int Xsize, int x, int y, unsigned int FRcolor, unsigned int BKcolor, unsigned char font)
{
    int i = 0, j = 0;
    unsigned int *addr = NULL;
    unsigned char *fontp = NULL;
    int testval = 0;
    fontp = font_ascii[font];

    for (i = 0; i < 16; i++)
    {
        // 向帧缓冲区的相应地址写入数据，就可以点亮一个像素，我们用4个字节来表示1个像素的颜色
        addr = fb + Xsize * (y + i) + x;
        testval = 0x100;
        for (j = 0; j < 8; j++)
        {
            testval = testval >> 1;
            if (*fontp & testval)
                *addr = FRcolor;
            else
                *addr = BKcolor;
            addr++;
        }
        fontp++;
    }
}

// 打印一个字符串，字符串的显示位置随Pos而确定
// 当遇到\n字符时，换行
// 当遇到\b字符时，用空格代替前一个字符，并将下一个字符显示位置回退1
// 当遇到\t字符时，按制表符对齐
int color_print_string(unsigned int FRcolor, unsigned int BKcolor, const char *str)
{
    int len = strlen(str);
    int i = 0;
    for (i = 0; i < len; i++)
    {
        if (str[i] == '\n')
        {
            // 换行
            Pos.YCharPosition++;
            Pos.XCharPosition = 0;
        }
        else if (str[i] == '\t')
        {
            // 按制表方式对齐
            //  (Pos.XPosition+8) & 11111111 11111111 11111111 11111000,相当于向上取整为8的倍数，再减去原先的Pos.XPosition的值，得到的就是\t对应的空格数量
            // 这里的意思是，\t如果出现在行尾，比如一行80个字符，\t出现在第79个字符，那么第79和第80个字符替换为空格，而不是强制把\t替换成8个空格
            int SpaceOfTab = ((Pos.XCharPosition + 8) & ~(8 - 1)) - Pos.XCharPosition;
            while (SpaceOfTab--)
            {
                putchar(Pos.FB_addr, Pos.XResolution, Pos.XCharPosition * Pos.XCharSize, Pos.YCharPosition * Pos.YCharSize, FRcolor, BKcolor, ' ');
                Pos.XCharPosition++;
            }
        }
        else if (str[i] == '\b')
        {
            // 删除前一个字符
            Pos.XCharPosition--;
            if (Pos.XCharPosition < 0)
            {
                // 当下一个字符的显示位置位于一行的最开始位置时，要删除前一个字符，那么就需要把显示位置回退到上一行的末尾字符显示位置
                Pos.XCharPosition = (Pos.XResolution / Pos.XCharSize - 1) * Pos.XCharSize;
                Pos.YCharPosition--;
                // 如果下一个字符的显示位置位于0行0列，那么说明待删除位置位于最后一行的最后一列
                if (Pos.YCharPosition < 0)
                    Pos.YCharPosition = (Pos.YResolution / Pos.YCharSize - 1) * Pos.YCharSize;
            }
            // 用空格代替需要显示的字符
            putchar(Pos.FB_addr, Pos.XResolution, Pos.XCharPosition * Pos.XCharSize, Pos.YCharPosition * Pos.YCharSize, FRcolor, BKcolor, ' ');
        }
        else
        {
            // 显示普通字符
            putchar(Pos.FB_addr, Pos.XResolution, Pos.XCharPosition * Pos.XCharSize, Pos.YCharPosition * Pos.YCharSize, FRcolor, BKcolor, str[i]);
            Pos.XCharPosition++;
        }

        if (Pos.XCharPosition >= (Pos.XResolution / Pos.XCharSize))
        {
            Pos.YCharPosition++;
            Pos.XCharPosition = 0;
        }
        if (Pos.YCharPosition >= (Pos.YResolution / Pos.YCharSize))
        {
            Pos.YCharPosition = 0;
        }
    }
    return len;
}

// 将一个无符号整型转化为对应字符串,支持2-36进制的转换
static void itoa(uint64_t value, char **buf, uint8_t base)
{

    if (value == 0)
    {
        // (*buf)[0] = '0';     // 曾经这样写导致打印整型0的时候显示不出来
        // (*buf)[1] = '\0';
        *(*buf)++ = '0';
        return;
    }
    // 64位整型，使用65个字符足以表示了
    char tmp_buf[65] = {0};
    int i = 0;

    while (value)
    {
        uint64_t m = value % base;
        tmp_buf[i++] = (m < 10) ? (m + '0') : (m - 10 + 'A');
        value = value / base;
        int k = i;
    }
    for (int j = i - 1; j >= 0; j--)
    {
        *(*buf)++ = tmp_buf[j];
    }
    (*buf)[i] = '\0'; // 其实这一句是多余的，因为buf中的数据都默认初始化为0了，不过显式表示出来是为了提醒注意字符串后面追加'\0'字符
}

// vsprintf函数的作用是把格式化字符串中的格式化字符替换为实际参数，新的字符串保存在buf中
int vsprintf(char *buf, const char *format, va_list args)
{
    char *str = buf;
    const char *index_ptr = format;
    char index_char = *index_ptr;
    int64_t arg_int64;
    int64_t arg_uint64;
    char *arg_str;
    while (index_char)
    {
        if (index_char != '%')
        {
            *(str++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }
        index_char = *(++index_ptr);            // 得到%后面的字符
        switch (index_char)
        {
            case 's':
                arg_str = va_arg(args, char*);
                strcpy(str, arg_str);
                str += strlen(arg_str);         // 注意修改str的值
                index_char = *(++index_ptr);
                break;
            case 'c':
                // 注意这里传入的本应该是char，但是gcc会报警告，char被提升为int
                *(str++) = va_arg(args, int); // (*str)++会提示表达式是不可修改的左值，这是一个值得研究的问题，查看其汇编代码是怎么样的，和*(str++)的汇编代码有什么区别？
                index_char = *(++index_ptr);
                break;
            case 'd':
                arg_int64 = va_arg(args,int);
                if (arg_int64 < 0)
                {
                    arg_int64 = -arg_int64;
                    *str++ = '-';
                }
                itoa(arg_int64, &str, 10);
                index_char = *(++index_ptr);
                break;
            case 'x':
                arg_uint64 = va_arg(args, unsigned long);
                itoa(arg_uint64, &str, 16);            // 注意需要需要更新str的值，因此传入str的地址
                index_char = *(++index_ptr);
                break;
            
        }
    }
    return strlen(buf);
}

int printf(char *format, ...)
{
    char buf[1024] = {0};           // 这里曾经没有初始化，导致打印结果不正确，原因在于多次调用printf时，栈中残留着之前的字符串数据
    va_list args;
    va_start(args, format);             // 曾经忘记过将args初始化，导致错误
    int count = vsprintf(buf, format, args);
    va_end(args);
    color_print_string(RED, BLACK, buf);
}
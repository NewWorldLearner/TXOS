
#include "print_kernel.h"
#include "font.h"

struct position Pos;

void init_screen()
{

	Pos.XResolution = 1440;
	Pos.YResolution = 900;

	Pos.XCharPosition = 0;
	Pos.YCharPosition = 0;

	Pos.XCharSize = 8;
	Pos.YCharSize = 16;

	Pos.FB_addr = (int *)0xffff800000a00000;
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


int strlen(const char* str)
{
    const char *ch=str;
    int len=0;
    while(*ch++)
    {
        len++;
    }
    return len;
}


// 打印一个字符串，字符串的显示位置随Pos而确定
// 当遇到\n字符时，换行
// 当遇到\b字符时，用空格代替前一个字符，并将下一个字符显示位置回退1
// 当遇到\t字符时，按制表符对齐
int color_print_string(unsigned int FRcolor, unsigned int BKcolor, const char *str)
{
    int len=strlen(str);
    int i = 0;
    for (i = 0; i < len ;i++)
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
            while(SpaceOfTab--)
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

int print_str(const char* str)
{
    return color_print_string(RED, BLUE, str);
}

// int print_hx16(unsigned char num)
// {
//     char low = num % 16;
//     char high = num /16;
//     if (low >=0 && low <= 9)
//     {
//         low += '0';
//     }
//     else
//     {
//         low +='A';
//     }
//     if (low >= 0 && low <= 9)
//     {
//         high += '0';
//     }
//     else
//     {
//         high += 'A';
//     }
//     print_str("0x");
//     print_str()
// }
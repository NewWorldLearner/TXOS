#ifndef _PRINT_KERNEL_H_
#define _PRINT_KERNEL_H_

#define WHITE 	0x00ffffff		//白
#define BLACK 	0x00000000		//黑
#define RED	0x00ff0000			//红
#define ORANGE	0x00ff8000		//橙
#define YELLOW	0x00ffff00		//黄
#define GREEN	0x0000ff00		//绿
#define BLUE	0x000000ff		//蓝
#define INDIGO	0x0000ffff		//靛
#define PURPLE	0x008000ff		//紫

/*

*/

extern unsigned char font_ascii[256][16];

struct position
{
	int XResolution;    // 屏幕长度
	int YResolution;    // 屏幕宽度

	int XCharPosition;      // 字符显示位置，单位是字符的大小
	int YCharPosition;

	int XCharSize;      // 字符的宽度
	int YCharSize;      // 字符的高度

	unsigned int * FB_addr;     //帧缓冲地址
	unsigned long FB_length;    // 帧缓存长度
};

void init_screen();

void putchar(unsigned int *fb, int Xsize, int x, int y, unsigned int FRcolor, unsigned int BKcolor, unsigned char font);

int color_print_string(unsigned int FRcolor, unsigned int BKcolor, const char *str);

int printf(char *format, ...);

#endif
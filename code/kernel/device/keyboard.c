#include "../include/stdint.h"
#include "../include/interrupt.h"
#include "../include/io.h"
#include "../include/print_kernel.h"

// 下面这些字符显示不出来
#define esc '\033' // 八进制表示字符,也可以用十六进制'\x1b'
#define backspace '\b'
#define tab '\t'
#define enter '\n'
#define delete '\177' // 八进制表示字符,十六进制为'\x7f'

// 对于ctrl、shift、alt、capalock这几个键，它们不产生ASCII码，就把它们的ASCII码定义为0
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

// 定义控制字符的通码和断码
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

/* 以通码make_code为索引的二维数组 */
static char keymap[][2] = {
    /* 扫描码   未与shift组合  与shift组合*/
    /* ---------------------------------- */
    /* 0x00 */ {0, 0},
    /* 0x01 */ {esc, esc},
    /* 0x02 */ {'1', '!'},
    /* 0x03 */ {'2', '@'},
    /* 0x04 */ {'3', '#'},
    /* 0x05 */ {'4', '$'},
    /* 0x06 */ {'5', '%'},
    /* 0x07 */ {'6', '^'},
    /* 0x08 */ {'7', '&'},
    /* 0x09 */ {'8', '*'},
    /* 0x0A */ {'9', '('},
    /* 0x0B */ {'0', ')'},
    /* 0x0C */ {'-', '_'},
    /* 0x0D */ {'=', '+'},
    /* 0x0E */ {backspace, backspace},
    /* 0x0F */ {tab, tab},
    /* 0x10 */ {'q', 'Q'},
    /* 0x11 */ {'w', 'W'},
    /* 0x12 */ {'e', 'E'},
    /* 0x13 */ {'r', 'R'},
    /* 0x14 */ {'t', 'T'},
    /* 0x15 */ {'y', 'Y'},
    /* 0x16 */ {'u', 'U'},
    /* 0x17 */ {'i', 'I'},
    /* 0x18 */ {'o', 'O'},
    /* 0x19 */ {'p', 'P'},
    /* 0x1A */ {'[', '{'},
    /* 0x1B */ {']', '}'},
    /* 0x1C */ {enter, enter},
    /* 0x1D */ {ctrl_l_char, ctrl_l_char},
    /* 0x1E */ {'a', 'A'},
    /* 0x1F */ {'s', 'S'},
    /* 0x20 */ {'d', 'D'},
    /* 0x21 */ {'f', 'F'},
    /* 0x22 */ {'g', 'G'},
    /* 0x23 */ {'h', 'H'},
    /* 0x24 */ {'j', 'J'},
    /* 0x25 */ {'k', 'K'},
    /* 0x26 */ {'l', 'L'},
    /* 0x27 */ {';', ':'},
    /* 0x28 */ {'\'', '"'},
    /* 0x29 */ {'`', '~'},
    /* 0x2A */ {shift_l_char, shift_l_char},
    /* 0x2B */ {'\\', '|'},
    /* 0x2C */ {'z', 'Z'},
    /* 0x2D */ {'x', 'X'},
    /* 0x2E */ {'c', 'C'},
    /* 0x2F */ {'v', 'V'},
    /* 0x30 */ {'b', 'B'},
    /* 0x31 */ {'n', 'N'},
    /* 0x32 */ {'m', 'M'},
    /* 0x33 */ {',', '<'},
    /* 0x34 */ {'.', '>'},
    /* 0x35 */ {'/', '?'},
    /* 0x36	*/ {shift_r_char, shift_r_char},
    /* 0x37 */ {'*', '*'},
    /* 0x38 */ {alt_l_char, alt_l_char},
    /* 0x39 */ {' ', ' '},
    /* 0x3A */ {caps_lock_char, caps_lock_char}
    /*其它按键暂不处理*/
};

// 我们通常使用ctrl + c，ctrl + v这样的按键组合，因此我们记录ctrl、shift、alt、capslock这几个按键是否被按下
static bool ctrl_status, shift_status, alt_status, caps_lock_status;
// 扩展码是两个字节，因此也要记录是否为扩展码
static bool ext_scancode;

extern intr_handler idt_func_table[256];

// 处理按键的弹起
static inline void handle_key_release(uint16_t scancode)
{
    // 通码和断码的区别就在于，位8为1时是断码，位8为0时是通码
    uint16_t make_code = scancode & 0xff7f;
    // 检测到对应按键被弹起，将按键的状态置为false
    // 对于其它的按键，它们被弹起的时候什么也不需要做
    if (make_code == ctrl_l_make || make_code == ctrl_r_make)
    {
        ctrl_status = false;
    }
    else if (make_code == shift_l_make || make_code == shift_r_make)
    {
        shift_status = false;
    }
    else if (make_code == alt_l_make || make_code == alt_r_make)
    {
        alt_status = false;
    }
}

// 处理按键按下，返回扫描码对应的ASCII码
static inline char handle_key_press(uint16_t scancode)
{
    // 对于keymap以外的按键，不处理
    if (scancode >= 0x3b && scancode != alt_r_make && scancode != ctrl_r_make)
    {
        printf("unknown key!");
        return 0;
    }

    // 除了ctrl、shift、alt、capslock键，以及backspace、tab、enter、esc这样的按键按不按shift键都是同样的效果
    // 其它的键都受shift键的影响，字母键受capslock和shift键的影响（同时按下capslock和shift时等同于没按）
    // 因此判断一个扫描码被转化成什么ASCII码，首先要看是否按下了shift，然后才好从keymap表中查找对应的字符
    bool shift = false;
    // 接下来，通常字母键是我们最经常使用的，因此先处理字母键，字母键的扫描码范围如下
    if (scancode <= 0x10 && scancode <= 0x19 || scancode <= 0x1e && scancode <= 0x26 || scancode <= 0x2c && scancode <= 0x32)
    {
        if (shift_status && caps_lock_status)
        {
            shift = false;
        }
        else if (shift_status || caps_lock_status)
        {
            shift = true;
        }
        else
        {
            shift = false;
        }
    }
    // 对于其它的按键，它们对应的ASCII码只收shift键的影响
    else
    {
        shift = shift_status;
    }
    // 向alt这样的键分为左键和右键，效果都一样，但是右键前面有一个0xe0扩展码，消除0xe0之后，相当于使用左边的alt键，效果一样
    int index = scancode & 0xff;
    char cur_char = keymap[index][shift];
    // 如果扫描码对应的ASCII码不为0，说明它不是ctrl、shift、alt和cpaslock，直接返回ASCII码
    if (cur_char != 0)
    {
        return cur_char;
    }

    if (scancode == ctrl_l_make || scancode == ctrl_r_make)
    {
        ctrl_status = true;
    }
    else if (scancode == shift_l_make || scancode == shift_r_make)
    {
        shift_status = true;
    }
    else if (scancode == alt_l_make || scancode == alt_r_make)
    {
        alt_status = true;
    }
    else if (scancode == caps_lock_make)
    {
        // 每次按下capslock键时都是对之前的状态取反
        caps_lock_status = !caps_lock_status;
    }

    return 0;
}

// 键盘中断处理程序
void keyboard_handler(uint64_t rsp, uint8_t vec_no, uint32_t error_code)
{
    // 从8042的输出缓冲区读取一个字节
    uint16_t scancode = inb(0x60);

    // 发送EOI到8259A主芯片
    outb(0x20, 0x20);

    // 如果是扩展码的第1个字节，将标志位打开即处理完毕
    if (scancode == 0xe0)
    {
        ext_scancode = true;
        return;
    }
    // 如果上次读取的字节是0xe0，和这次读取的字节合并为一个扩展码
    if (ext_scancode)
    {
        scancode = ((0xe000) | scancode);
        ext_scancode = false;
    }

    // 判断是否为断码
    bool break_code = (scancode & 0x0080) != 0;
    if (break_code)
    {
        handle_key_release(scancode);
        return;
    }

    // 如果不是断码，那么就是通码咯，下面进行通码的处理
    char cur_char = handle_key_press(scancode);

    if (cur_char > 0)
    {
        printf("%c",cur_char);
    }

}

/* 键盘初始化 */
void keyboard_init()
{
    printf("keyboard init start\n");
    // 其实键盘的输入应该保存在一个输入缓冲区，以便后续可以使用，比如shell命令就需要读取一个字符串，但是我们先不完成这个
    
    // 这里我们还是简单使用8259A作为中断代理
    idt_func_table[33] = keyboard_handler;

    printf("keyboard init done\n");
}

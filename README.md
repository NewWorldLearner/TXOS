# 前言
记录一个64位操作系统开发过程的学习笔记

# 第1章 编写mbr程序

## 计算机的启动过程

1）计算机在启动的时候，CS的值为0xFFFF，IP的值为0X0000，因此执行的第一条指令的物理地址为0XFFFF0，该地址是BIOS程序的入口地址。该地址位于1MB内存的顶端，因此是一条跳转指令——jmp far f000：e05b。

2）开始执行BIOS程序：

* 检测硬件并初始化
* 在低1KB空间建立中断向量表
* 检查磁盘的第一个扇区是否为MBR（要求该扇区的最后两个字节必须为0x55和0xaa），若满足则加载MBR到0x7c00物理地址处，然后执行跳转指令jmp 0:0x7c00。

3）执行MBR程序



## mbr程序的设计思路

mbr程序存放于硬盘的第1个扇区，只要该扇区的最后两个字节为0x55和0xaa即可，因此我们可以很轻松的编写出一个符合mbr格式的程序。
但是mbr程序的作用是跳转到loader程序去执行，因此mbr程序需要将loader程序加载到内存中，并跳转到loader程序去执行。
因此，列出mbr程序的主要作用如下：
1）将loader程序加载到内存中
2）跳转到loader程序去执行

我们可以固定loader程序在磁盘中的扇区地址，然后在mbr中去读这个指定的扇区，但是我们不使用这种方法，不是很灵活。
我们使用的方法是，直接在磁盘中创建一个fat16文件系统，将来我们直接在磁盘中搜索loader文件，并将它读取到内存中就可以了。

我们还需要考虑哪些问题呢？
mbr程序使用的栈空间
loader程序的加载地址，注意在实模式下我们只能使用1MB内存——常使用的内存位置是64KB处，也就是0x1000:0x00
bochs虚拟机初始会显示一些信息，因此我们需要清屏
我们想要在虚拟机中打印一些提示信息，因此我们需要调用bios中断来显示字符串，这个过程可能需要把光标的位置设置到屏幕开始
要把扇区中的根目录读取到内存中，因此还需要确认该内存地址


## fat16文件系统
一篇fat16文件系统的参考文章：
https://blog.csdn.net/yeruby/article/details/41978199


## mbr程序的编写
1）在扇区的开头定义一个fat16文件系统
2）清空屏幕，否则的话屏幕上会有一些bochs相关的信息
3）设置关闭位置，在第1行打印boot引导的信息提示
4）在磁盘中搜索loader.bin文件，如果存在的话，将loader加载到内存位置0x1000:0x00处
    需要加载根目录扇区到内存中，常见的位置是0x8000处
    遍历根目录中的目录项，如果存在loader.bin，将其加载到内存中，否则的话打印文件不存在的信息
    跳转到0x1000：0x00处执行loader程序

我在这个过程中遇到了一个困难，当我把loader文件拷贝到磁盘中的时候，发现该磁盘中只保存了文件名，但是文件内容却是空的，我尝试了几天都没有找到解决办法，也许可能是我的fat16文件系统写得有问题，也有可能是其它的问题，但是我决定不再浪费时间在这里了。
因此，我写了一个新的mbr程序，这个程序不再包含fat16文件系统，它的主要作用加载loader程序并直接跳转到loader程序执行，loader程序所在扇区的位置直接进行硬编码。

我似乎找到了原因，应该是需要将磁盘从linux系统中取消挂载才可以？？？待后续验证一下

# 第2章 编写loader程序

由于mbr程序只能512字节，因此很多功能是无法塞到mbr中的，所以mbr程序的主要作用就是跳转到loader程序执行。
loader程序的主要作用如下：
1）检测硬件信息，比较重要的是物理内存信息，VBE功能等。
2）处理器模式切换，需要从实模式切换到保护模式，再切换到64位模式
    32位保护模式需要GDT表，64位模式也需要相应的GDT表，因此进入32位保护模式时，需要创建一个32位保护模式使用的GDT表作为过渡
3）向内核传递信息，一类是控制信息，另一类硬件数据信息
4）加载内核程序，并跳转到内核执行
    低1MB内存的内存布局挺复杂的，因此我们把内核安排到1MB内存的开始处
    加载内核程序时，由于内核程序是由c语言写的，编译出来的文件并不是纯二进制文件（elf文件），要么我们可以提取出二进制代码（简单优选），要么我们对elf文件进行解析
    加载内核程序可以安排到进入64位模式以后，也可以安排到在实模式下进行，为了简单起见，在实模式下就加载内存吧

我们的loader程序将围绕以上功能进行展开。
程序执行流如下：
1）临时进入保护模式，使fs暂时拥有1MB以上内存的寻址能力，然后退出保护模式
2）将内核加载到1MB内存处
3）检测内存信息
4）检测VGA信息
5）进入64位模式
6）跳转到内核执行


## 加载内核

我在实模式下想将内核加载到1MB内存处，我直接使用ebx作为内存偏移量，在调用磁盘读取函数的时候，程序莫名奇妙的跑飞，而且在真正读入磁盘数据之前（等待读取磁盘时）就跑飞，不知道这个是什么原因。
我临时进入保护模式，然后又退出，使fs寄存器临时拥有1MB以上内存的寻址能力。然后我尝试了使用[fs:ebx]作为内核的加载地址，发现程序依然会跑飞。
现在我只有在保护模式下将内核加载到1MB内存处。

进入保护模式的3个步骤：
1）打开A20地址线
2）加载GDT
3）将CR0的pe位置1
上述三个步骤可以不按顺序，也可以不连续完成。

在保护模式下，每个段都是保存在GDT中的，GDT被存放在内存中的某个地址，其地址和大小保存在GDTR寄存器中。
GDT中保存的是段描述符，每个段描述符的大小为8个字节，第0个段描述符不能被使用。
段寄存器中保存的是段选择子，其3-15位表示的是要访问的段描述符表中段描述符的索引。位0-1表示的是RPL，位2表示要访问的是GDT还是LDT——0表示GDT，L表示LDT。



## 检测物理内存
BIOS有3种方式可用检测物理内存，最灵活的是调用int 15h AX=E820号功能。调用该功能时，每次会返回一段可用内存，多次调用该功能就可用知道总的可用物理内存。
该功能每次返回20字节大小的信息来表示检查到的物理内存段，称为地址范围描述符（ARDS）
0-3字节：表示基地址的低32位
4-7字节：表示基地址的高32位
8-11字节：表示内存长度的低32位
12-15字节：表示内存长度的高32位
16-19：表示本段内存的类型，值为1表示这段内存可用被操作系统使用，值为2表示内存使用中或操作系统不可使用该段内存，值为其它未定义


int 15h Ax=E820号功能使用方式如下：

调用前输入：
EAX用于指定使用的的功能号，EAX=0xE820
EBX是ARDS的后续值：初始为0，当检测完所有可用内存段后，EBX为0，ARDS标识每次返回的地址范围描述符的结构体
ES:DI：每次返回内存信息的写入地址，每次都以ARDS格式返回
ECX：ARDS结构的字节大小，固定为20
EDX：固定为0x534d4150，是SMAP的ASCII码

调用后输出：
CF位：若为0则调用未错误，否则调用出错
EAX：SMAP的ASCII码，0x534d4150
ES:DI：ARDS缓冲区地址，同输入前是一样的，可用不关注
ECX：写入到缓冲区的字节数
EBX：下一个ARDS的位置，当它为0时，表示已经检测完所有的内存段

在我们的操作系统设计中，我们指定的ARDS缓冲区的地址为0x7E20，指定ARDS结构体的数量保存在0x7E00处

## 设置VBE

1）线性帧地址保存在ModeInfoBlock中，此物理地址不能直接使用，需要将该物理地址映射到线性地址空间，然后将线性地址空间映射到程序地址空间才能使用。
2）刷新率的控制，CRTC
3）24位或32位真彩色（32位要求至少2MB物理内存）

VBE功能调用的共同点：
1）AH必须等于4FH，表明是VBE标准；
2）AL等于VBE功能号，0<= AL <= 0BH；
3）BL等于子功能号，也可以没有子功能；
4）调用INT 10H；
5）返回值

VBE返回值一般在AX中：
1）AL=4FH：支持该功能
2）AL!=4FH：不支持该功能；
3）AH=00H：调用成功；
4）AH=01H：调用失败；
5）AH=02H：当前硬件配置不支持该功能；
6）AH=03H：当前的显示模式不支持该功能；

VBE控制信息——AL=0x00号功能
VBE模式信息——AL=0x01号功能
获取当前VBE模式——AL=0x02号功能

我在获取VBE控制信息后，得到一个指针，指向VBE支持的模式数组，但是遍历该模式数组并获取模式信息时，在某次遍历中虚拟机突然卡死了，不知道原因是什么。
是因为bochs并不支持所有的模式号吗，即使该模式号是检测出来的？
记录断点 0x1000：0x01bf
后面我发现无需遍历和检测VBE模式，再查询VBE控制信息之后，就可以通过查看距离VBE模式起始地址偏移量为0x0e的地方保存着一个指针，该指针指向的是模式数组，直接通过查看内存信息，就可以知道VBE所有的模式了。

查看bochs虚拟机支持的显示模式，发现比较适合的是0x183，设置显示模式之后，VBE图形模式会禁用BIOS文本模式缓冲区(0xB8000)，因此不能再通过int 10中断来显示字符串。执意这样做的话，程序直接跑飞了。

设置VBE的显示模式之后，虚拟机的显示界面已经发生改变。




## 进入64位模式

首先关闭中断
进入64位模式时，首先进入保护模式，由于之前已经打开了A20地址线，因此本次进入保护模式只需要加载GDT和将CR0的pe位置1。
进入保护模式之后，首要任务是初始化各段寄存器
进入64位模式之前，需要检测处理器是否支持64位模式，需要用到cpuid指令

在进入保护模式并通过跳转指令刷新CS的时候，我发现程序出现了错误，程序跑飞到了0xf000:0xfff0这个地方，这是为什么？跑飞前执行的指令是jmpf 0x0008:0x0224
是因为我的GDT表设置得不正确吗？我检查了GDT表，应该没有错误，代码段的基地址是0x00，那么上面跳转指令的实际效果就是跳转到0x0224，检查一下这个内存地址的反汇编代码，检查反汇编代码,发现没有问题。
我找到了原因，因为在loader程序中定义了多个段，而我在第一个段中定义了vstart=0x10000，但是其它段并没有vstart定义，因此其他段中的地址并不正确，这也就是通过jmp指令刷新保护模式下的CS寄存器时，偏移量是0x0224，而不是0x10224的原因，这就导致了程序跑飞。

在进入保护模式时，检测bochs是否支持64位模式，检测结果居然是不支持。在调试过后，我发现我使用的bochs虚拟机是不支持长模式的，我使用的版本是2.6.11，可能是我编译的时候没有选择支持长模式，重新编译一下试试。
重新编译时，加上了--enable-x86-64这个编译选项，调试时能够成功进入64位模式了。


进入64位模式的标准步骤：
1）在保护模式下，将CR0的PG位复位
2）置位CR4寄存器的PAE标志位
3）将页目录（4层页表）的物理地址加载到CR3寄存器中
4）置位IA32_EFER的LME标志位，开启64位模式
5）置位CR0的PG位(31位)和PE位(位0)，此时处理器会自动置位IA32_EFER的LMA标志位

如果我们在进入64位模式后，再关闭这些相应的标志位，会导致#GP异常

这里我们先拓展一些知识：

4级页表：
在64位系统下，最大寻址能力为64位，但是当前只使用到的最大寻址位数为48位。
为了对48位地址进行寻址，使用的是4级页表。
一个页占据4KB的物理页面，由于页表中存放的地址是64位的，需要占据8个字节，因此一页只能存放512个页表项。
对于48位地址，我们就可以这样划分，47-39作为页全局目录，38-30作为页上级目录，29-21作为页中级目录，20-12作为页表。
在4级页表的模式下，对48位地址寻址是这样的：
    1）取47-39位并乘以8作为页全局目录中的偏移量，得到页上级目录的物理地址。
    2）取38-30位并乘以8作为页上级目录中的偏移量，得到页中级目录的物理地址。
    3）取29-21位并乘以8作为页中级目录中的偏移量，得到页表的物理地址。
    4）取20-12位并乘以8作为页表中的偏移量，得到线性地址对应的物理页。
    5）0-11位作为物理页中的偏移量得到线性地址对应的物理地址。

在64位模式下，一个物理页可以是4kB，2MB和1GB（需要检测处理器是否支持1GB的物理页）

对于4KB物理页来说，我们知道页地址的低12位全为0，因此我们可以考虑把这低12位用来表示其它信息

对于页全局目录来说，只需要一个4KB的页就可以表示了
对于页上级目录来说，它需要512个4KB页来表示，我们需要提前准备这些内存吗？
对于页中级目录来说，它需要512*512个4KB页来表示

一个页中级目录中的一项指向一个页表，一个页表有512项，也就是一个页表代表2MB内存，也就是一个页中级目录中的一项代表2MB内存。
页上级目录中的一项代表2MB*512=1GB内存
页全局目录中的一项代表1GB*512=512G内存

# 第3章 编写内核程序

程序能够成功进入64位模式，并且也能够跳转到1MB内存存，但是不知道为什么，1MB内存处的代码并不是我写的程序，这是什么原因？
我在调试的时候，是成功的读取了磁盘的，但是1MB内存处的数据反汇编结果却不对，原因是什么呢？检查了磁盘中对应的扇区确实是写入的内核内容，那么就是磁盘读取程序不对。
我在调式的时候，发现读取磁盘的前2个字节是正常的，但是当读取完磁盘的时候，结果就不正确了……
经过单步调试，我发现磁盘是正确的被读入到了1MB内存处，但是反汇编出来的结果和我在内核中写的汇编代码不同，是因为我汇编得不对吗？

我只是想简单的测试一下内核程序程序能否被加载到1MB内存处，因此我的内核程序如下：
org 0x100000
mov eax,0
mov eax,0
mov eax,0
jmp $

db "start kernel"

我想到，这是因为nasm编译器默认以16位操作数尺寸来编译，而我却是在64位模式下运行的吗？
我指定[BITS 64]后再用nasm编译，并没有解决问题。
好吧，我的目的只是验证内核是否能成功的加载到1MB内存处，经过我的调试，看来是成功的加载到了1MB内存处，先不要纠结为什么用nasm编译不正确了。

现在我面临的问题是，编译出来的内核代码并不能按照预期执行，反汇编出来的结果有问题，那么是否是因为我进入64位模式的代码写得有问题呢？
我需要尝试一下，直接对磁盘代码进行反汇编，反汇编出来的结果是正确的。

我最终解决了这个问题，原因是要用bits 64指令来指定汇编成64位代码，之前我已经想到这个问题，但是当时没有验证出来，应该是我实际编译出来的代码并没有被写入到磁盘中，因为路径写得不正确，我一直写入的都是别处的代码，大概原因是这个吧。这个问题花费了我几个小时的时间，反复的尝试各自办法，大概是我已经把自己的脑子弄混乱了吧。我想以后写代码还是多明确指定一下路径吧。

## 内核执行头程序
为了让GDT表，IDT表和TSS_64表能够被外部程序访问，因此我们在内核执行头中重新定义了这几个表，并使用伪指令导出相应的变量（如果不指定的话，直接给相应变量赋值也可以达到这个目的吧）。

在执行内核执行头程序的时候，我遇到一个问题，程序在执行一条赋值语句之后竟然跑飞了。不知道是什么原因，错误出现在哪里。


我把前面的语句注释掉之后，在执行到用eax给cr3寄存器赋值的时候，提示物理地址不可用。为什么这里会崩溃？尝试了将原先的页全局目录地址付给cr3，没有崩溃，也许是执行头中的页全局目录设置得有问题。在调试的时候，查看了页全局目录处的信息，没有发现错误。太奇怪了，我先避开这个问题。
但是仍然无法解决程序会在执行下面的代码后直接跑飞：
	mov	$0x10,	%ax		
	mov	%ax,	%ds

我检查了GDT的表项，确认是没有问题的，我将重新加载GDT的指令注释掉，然后程序就能够成功执行下去了。那么问题是什么呢？问题可能出现在链接不正确吗？链接不正确，导致加载的GDT的地址实际上是有问题的？但是查看了一下链接方式，看不出来有任何问题……
后来我发现调试的时候，lgdt ds:[rip+2117744],非常奇怪，gdt相对于rip的位置怎么会这么远？
[shy@localhost kernel]$ objdump -d system | grep "lgdt"
ffff800000100011:       0f 01 15 b0 40 00 00    lgdt   0x40b0(%rip)        # ffff8000001040c8 <GDT_END>
上面是我反汇编出来的代码，明显和我在bochs中反汇编出来的地址有区别，在内存中的地址明显有点不太对

让我再看看GDT编译出来之后的位置
[shy@localhost kernel]$ objdump -t system | grep "GDT_POINTER"
ffff8000001040c8 l       .data  0000000000000000 GDT_POINTER

我突然意识到，是不是我的磁盘读取有问题，我重新检查了加载内核的代码，发现我加载内核时指定的扇区居然只有16，我立刻将它改成了255，然后调试就成功了……

-----------------------------------------------------------------------------------
我一开始觉得直接将编写好的内核文件提取出纯二进制文件会简单一点，但是由于不清楚是否是链接的问题导致GDT的位置和页全局目录的地址不对，因此我决定在loader中直接手动解析elf文件，将内核的二进制代码直接加载到1MB内存处，我得仔细考虑一下整个内核的内存布局。

考虑一下该操作系统支持的最大内存，就32GB吧
页全局目录中的一项代表512GB，因此页全局目录占4KB
页上级目录中的一项代表1GB，因此页上级目录占4KB
一个页中级目录代表1GB，因此需要4个页中级目录
页中级目录中的一项指向一张页表，因此4个页中级目录中共2048项，也就是需要2048个4kb的页表，这需要占据的内存有8MB
因此，要么把页目录安排在1MB内存处，要么使用2MB的物理页，或者操作系统在后面动态申请页表

为了简单起见，就使用2MB的物理页吧，这样就能够提前安排好页目录的位置，使其在0x9000处

再考虑一下GDT表应该安排在哪里
………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………………
我终于找出了问题所在，原来是我加载内核时指定的扇区数量为16，导致内核程序并没有完全加载到内存中，所以出现了很多稀奇古怪的错误……



# 第4章 实现常用库函数

## 实现打印字符的功能
我们使用的是VBE功能来控制屏幕，VBE功能中一个像素用4字节来表示，使用24位像素模式，第1字节表示蓝色，第2字节表示绿色，第3字节表示红色，第4个字节保留不使用。

我现在碰到的问题是，bochs虚拟机并不能正确显示图像，我选用的VBE模式号是0x143

检查VBE对应的物理地址：0xe0000000,地址是正确的
检查了帧缓存区的内存0xffff800000a00000，发现全是0xffffffff，但是在调试的过程中明明对这个区域进行赋值了，为什么还是错误的？
检查了页表的映射，是正确的
我检查了bochs虚拟机的内存大小，我原先设置的是256MB，但是0xe0000000这个物理地址对应的是3584MB,但是显存的地址也属于内存映射，应该和物理内存无关
现在的问题是，对帧缓冲区赋值了，但是不知道为什么没有生效？
比较奇怪的是，我通过0xffff800000a00000查看该内存位置的值全为0xff，但是通过0xa00000查看该内存位置的值全为0，明明这两个线性地址映射的物理内存都是一致的，为什么结果会不相同呢？我还尝试了，对其它区域的内存赋值是可以的。
现在，我的猜想是，因为bochs不支持我使用的VBE模式号，所以才无法对帧缓存区赋值吗？
---------------我现在无法解决这个问题-------------------------

要么是bochs配置有问题，要么是我的bochs版本有问题，考虑更换bochs版本试试

-----------------我确认了，可能是磁盘配置的问题，因为我使用软盘的时候，程序是能够正常执行的，但是我现在无法确认硬盘配置到底哪里出现了问题，我已经将硬盘的配置改得和软盘几乎一致了-------------------------
我终于解决了这个问题，现在可以确认是磁盘配置的问题，但是我还不清楚到底是缺少了什么配置引起的，我需要好好对比一下现在的配置和之前的配置的差异，然后确认是由缺少哪一项配置引起的。
？？？我把磁盘的配置复原了，但是同样可以正常的显示出图案，也就是说，不能显示图像的问题莫名其妙的好了？
这究竟是什么原因？中间linux磁盘满了，我清理过一次linux磁盘，把bochs下的占4GB空间的日志删除了，会是由于这个原因导致的吗？
我验证了一下，拷贝大量文件来占满磁盘空间，发现并没有影响bochs的图像显示，那么我考虑一下，是因为bochs的日志占用4GB内存，每次往日志里追加内容都需要耗费大量的时间，从而影响到了图像显示吗？
现在我只能怀疑是日志的问题了，可惜在删除日志之前没有看过里面的内容……


VBE屏幕：
模式号180——x：1440，y：900
模式号0x143——x：800，y：600

要实现字符打印的功能，首先就需要通过像素来描绘出字符，使用一个8*16的矩阵来描绘一个字符，一个字节占8位，因此我们可以使用一个字节来表示字符矩阵的一行，然后使用16个字节就可以描述字符矩阵的每一行了，因此可以使用一个16字节的一维数组来描绘一个字符。为了表示ASCII的128个字符，可以使用一个128*8的二维数组。

接下来我们设计如何显示一个字符：put_char
需要指定字符的显示位置（x,y），显示位置都设为像素偏移量，当然显示位置肯定还和屏幕的长度有关
需要指定字符的字体颜色和背景颜色
需要指定字符的大小，当然我们先默认大小为8*16个像素

设计了put_char函数之后，我们就可以设计一个打印字符串的函数:
我先实现一个简单显示字符串的功能吧，至于格式化字符串，后续再考虑
设计的打印字符串的函数叫做color_print_string
如果后续要设计格式化打印字符串的函数，只需要先将格式化字符串转换为普通字符串，然后调用该函数即可

我在编写程序的过程中，遇到了错误，但是bochs只能进行汇编代码的调试，看看后续是否要替换为qemu。


## 实现格式化打印字符串的功能
格式化打印字符串的关键是实现可变参数，在32位模式下，gcc传递参数是通过压栈实现的，这样使用可变参数会比较容易。在64位模式下，gcc在参数小于等于6个时，使用的是寄存器传参，如果参数超过6个，超过6个的部分使用的是压栈来传递参数，这样会导致使用可变参数变得复杂一些吗？也许后面我可以尝试自己试一下。
现在的话，直接包含头文件<stdarg.h>即可使用可变参数

我要实现的是printf函数，它的原理是将格式化字符串中的格式化字符替换为实际的参数（实现这个功能的函数是vsprintf），这样打印再替换后的新字符串就实现了printf。

首先实现%s，%c，%d，%x这几个较为简单的格式化参数，后续其它的再进行补充。


## 实现字符串函数
较为简单，不再赘述。


## 实现断言
在开发的过程中，我们常常会遇到这些错误，于是我们希望使用断言。
断言传入的条件为真时，什么也不做，条件为假时，报错。
于是，一个非常简单的实现断言的方式就是在条件为假时，打印一条报错信息。但是，从语言层面上来说，我们无法知道是因为什么条件报错的，如果多个地方都使用了断言，那么我们将很难确定是什么地方发生了错误。
```
assert(bool condition)
{
    if (!condition)
    {
        printf("Assert Error\n");
        while(1);
    }
}
```

我们希望在断言遇到条件为假时，告诉我们错误发生在哪个文件，哪一行，哪个函数，错误条件是什么，显然这是从语言层面无法做到的，因此，我们需要编译器的帮助，有几个编译器预定义的宏如下：
_FILE_宏会告诉我们被编译的文件名
_LINE_宏会告诉我们被编译文件中的行号
_func_宏会告诉我们被编译的函数名

#CONDITION可以将传给宏的参数CONDITION变成字符串常量


# 第5章 内存管理

## 管理所有的内存页
在loader中，我们通过在实模式下调用BIOS中断来检测可用的物理内存段信息，并把它们放在了内存0x7E起始处的内存。现在，我们就需要规划和使用检测到的物理内存段的信息。
对于所有的物理页，我们可以使用一个位图来表示这些物理页，为了丰富对物理页的描述信息，对于每个物理页我们额外附加了一个Page结构体来描述该物理页。
我们都知道，物理内存实际上可能被划分为多个段，为了方便管理这些内存段，以及为了方便同时管理多个内存页，因此我们引入Zone结构体用来描述一段内存区域。

对于可用的内存段，我们使用一个Zone结构体来描述该内存段——对于内存段来说已经被向上2MB对齐以及向下2MB对齐
对于可用的物理页，我们使用一个Page结构体来描述该内存页

我们使用位图来描述所有物理内存页，将位图放置在程序结尾处的向上取整4KB处的起始位置
我们使用page数组来记录所有的物理页的属性，将Page数组放置在位图后面的向上取整4KB处的起始位置
我们使用Zone数组来记录所有的zone区域的属性，将Zone数组放在Page数组后面的向上取整的4KB处的起始位置

对于不可用的物理页，我们将对应的位置1；对于可用的物理页，我们将所有的物理页置0。
在完成对物理页的初始化之后，我们还需要把操作系统占用的物理页全部置1

我们把上面的操作需要使用到的信息封装成一个结构体，现在我们通过这个结构体就能够管理所有的内存页了。

## 内存页的分配和回收
我们已经可以通过一个结构体来描述所有的物理页，现在我们就可以通过该结构体来管理物理页的分配和回收。
因此，我们可以实现物理页的分配算法和回收算法。

在测试的过程中，发现打印到屏幕上的字符串一闪而过，不知道是什么地方出现了bug。bochs在后面直接崩溃了。
经过打印日志，发现似乎是检测物理内存的时候出错了，检测出来的物理内存段居然为0，具体的错误在哪里还需要查找。
经过调试发现，我将ARDS结构体的数量放在0x7E00处，但是在使用该内存的时候，使用的是默认段寄存器ds，其值为0x1000，于是导致ARDS结构体的数量并没有正确放到0x7E00处，导致后续取ARDS结构体数量的时候，得到的结果为0。
得到的经验，在开发的过程中，多打印日志，查找错误的时候很有用。

不知道为什么，bochs虚拟机设置的内存为2GB，但是实际上检测出来了约4GB。是否有地方隐藏着错误？
虽然bochs检测出来的最大内存为4GB，但是显示并不是可用的内存段……

## slab内存池技术

我们先思考一个简单粗糙的内存管理机制：
假设我们有一个数组，数组元素都指向一条链表，这条链表上中的结点都是空闲内存块，数组中的第0个元素代表对应链表上的内存块大小都是32字节，第1个元素代表对应链表上的内存块的大小都是64字节……
因此，我们需要申请对应内存块的大小时，只需要通过数组来找到对应的空闲内存块链接即可。
现在，我们来思考一下，如何向空闲链表中增加新的内存块呢？这个比较简单，首先申请新的内存页，然后划分成若干个内存块，只需要把空闲内存块当作结点插入到空闲链表中即可。
那么，如何释放已经使用的内存块呢？要知道返还内存块的时候，我们只知道内存块的地址信息，并不知到内存块的大小，因此根本不知道到底该释放多少内存。通过内存块的地址信息，我们能知道该内存块的页地址，如果想知道该页中的内存块的大小，那么就需要一个内存块大小信息（它属于元信息），知道内存块的大小以后，我们就知道该把该空闲块放到哪条空闲链表了。
以上，就可以实现内存块的分配和释放了。
现在再来思考另一种情况，如果同一时间申请了大量的内存，然后又释放掉了，这些空闲内存块都被加到空闲链表了，比如申请数千万个较小的内存块，然后又释放掉，现在想申请较大的内存块，但是物理内存已经被消耗完了，那么怎么办？因此我们还需要实现内存页的回收，也就是如果一个内存页中的内存块都被释放完了，要考虑能够回收该内存页。
要想知道一个内存页中的内存块什么时候被释放完，那么就需要引入内存块数量来记录一个内存页中总共有多少内存块，已经还剩余多少空闲内存块，这样当空闲内存块等于总的内存块时，就可以释放该内存页了。像剩余空闲块数量这样的数据是动态变化的，每个物理页面都不同，因此可以考虑和内存块尺寸这样固定不变的信息分开维护。
进一步再想一下，我们是通过链表来保存空闲内存块的，其实我们也可以通过通过链表来将内存页串联起来，这样的话，我们可以通过位图来标识内存页中的位图。


slab内存池技术：
slab是块的意思，因此slab其实就是把内存池中的内存分成等大的内存块的技术。

假设我们把一个物理页分成若干个内存块，这些内存块的状态在将来可能使用过，也可能没有使用过，因此我们就需要标记一下内存块的使用情况：
方法一：对于未使用过的内存块，我们用一个链表将它们链接起来
方法二：我们通过位图来标记内存块的使用

我们使用方法二
对于内存块，我们需要记录它的大小，还需要记录它所在的页面，记录所使用过的内存块，记录可用使用的内存块……因此我们使用一个结构体来维护这些相应的信息
当需要增加内存块的数量的时候，也就是说需要增加物理页的时候，毫无疑问我们是需要用一个链表来串联这些物理页的，如果把链表这个属性和其它属性一起放在一个结构体中进行维护，那么实际上每个链表都重复维护了内存块大小，使用过的内存块这样的元信息，这是没有必要的，因此我们再使用一个结构体来封装新的物理页的一些信息


## 实现内存块的分配和回收
通过slab内存池技术，我们可以划分出若干个内存池，每个内存池都使用一个内存池描述符来表示，每个内存池中的内存块大小不同，通过内存池我们已经能够实现内存块的分配和回收。

我在测试的时候，发现虚拟机执行到某段看起来很正常的代码崩溃了，经过一番查找，发现提示如下：
interrupt(long mode): gate descriptor is not valid sys seg
看起来是中断的问题，但是不知道中断是怎么触发的，于是我在内核执行头文件中添加sti指令，代码能够正常往下执行了，这个问题留着，后面我要看看到底是触发了什么中断
我又检测cli指令，发现在进入保护模式之前确实已经关闭中断了，难道进入长模式的时候，会自动打开中断，因此还要手动关闭中断？
再次查看似乎与cli指令无关，即使关闭了中断，执行到别的地方依然有相同的提示，经过查看，在保护模式下关闭中断以后，在长模式下依然是有效的
我将之前写好的中断系统引入，发现触发了缺页异常，到底是为什么会触发这个异常呢？

init_memory_slab函数似乎存在bug，bochs虚拟机崩溃了，很奇怪触发了缺页异常……我先放下这个bug吧，心累
后面使用qemu来进行调试

## 物理页和虚拟页的映射
在之前的开发中，使用的物理页都是在内核执行头中预先手动映射好的，对于所有的物理页，我们全部手动映射太麻烦了，因此现在要实现物理页的映射函数，帮助我们映射其它的物理页。
我们现在的开发还未涉及到用户进程的开发，因此映射的物理页都是给内核使用的。